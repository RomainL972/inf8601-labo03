/* NEGATIVE TEST FIXTURE (charge/): the classic bug, every rank sends first. Correct when the messages
 * are small enough to be buffered, a deadlock otherwise (and for a rank that is its own neighbour).
 * check-checksums must hang then time out: grade 0, nothing timed. */
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#include "heatsim.h"
#include "log.h"

/* Tags des messages: un par direction, ce qui permet à deux rangs voisins
 * dans plus d'une direction (grille 2xN) ou à un rang voisin de lui-même
 * (grille 1xN) de ne pas confondre les messages. */
enum {
    TAG_PARAMS = 0,
    TAG_DATA   = 1,
    TAG_NORTH  = 2,
    TAG_SOUTH  = 3,
    TAG_WEST   = 4,
    TAG_EAST   = 5,
    TAG_RESULT = 6,
};

/* Les paramètres d'une grille, envoyés en une seule requête. */
typedef struct grid_params {
    unsigned int width;
    unsigned int height;
    unsigned int padding;
} grid_params_t;

static int params_type_create(MPI_Datatype* type) {
    int blocklengths[3]       = {1, 1, 1};
    MPI_Aint displacements[3] = {offsetof(grid_params_t, width), offsetof(grid_params_t, height),
                                 offsetof(grid_params_t, padding)};
    MPI_Datatype types[3]     = {MPI_UNSIGNED, MPI_UNSIGNED, MPI_UNSIGNED};
    MPI_Datatype raw;

    if (MPI_Type_create_struct(3, blocklengths, displacements, types, &raw) != MPI_SUCCESS) {
        return -1;
    }

    /* Ajuste l'étendue sur celle de la structure C (rembourrage de fin). */
    if (MPI_Type_create_resized(raw, 0, sizeof(grid_params_t), type) != MPI_SUCCESS) {
        MPI_Type_free(&raw);
        return -1;
    }
    MPI_Type_free(&raw);

    if (MPI_Type_commit(type) != MPI_SUCCESS) {
        MPI_Type_free(type);
        return -1;
    }

    return 0;
}

int heatsim_init(heatsim_t* heatsim, unsigned int dim_x, unsigned int dim_y) {
    /* Communicateur cartésien périodique en X et en Y. Le rang 0 est
     * à (0, 0); la dimension 0 est Y (nord/sud), la dimension 1 est X. */
    int dims[2]    = {dim_y, dim_x};
    int periods[2] = {1, 1};
    int reorder    = 0;

    if (MPI_Cart_create(MPI_COMM_WORLD, 2, dims, periods, reorder, &heatsim->communicator) != MPI_SUCCESS) {
        goto fail_exit;
    }

    if (MPI_Comm_size(heatsim->communicator, &heatsim->rank_count) != MPI_SUCCESS) {
        goto fail_exit;
    }

    if (MPI_Comm_rank(heatsim->communicator, &heatsim->rank) != MPI_SUCCESS) {
        goto fail_exit;
    }

    if (MPI_Cart_coords(heatsim->communicator, heatsim->rank, 2, heatsim->coordinates) != MPI_SUCCESS) {
        goto fail_exit;
    }

    if (MPI_Cart_shift(heatsim->communicator, 0, 1, &heatsim->rank_north_peer, &heatsim->rank_south_peer) !=
        MPI_SUCCESS) {
        goto fail_exit;
    }

    if (MPI_Cart_shift(heatsim->communicator, 1, 1, &heatsim->rank_west_peer, &heatsim->rank_east_peer) !=
        MPI_SUCCESS) {
        goto fail_exit;
    }

    return 0;

fail_exit:
    return -1;
}

int heatsim_send_grids(heatsim_t* heatsim, cart2d_t* cart) {
    /* Pour chaque rang, deux requêtes non bloquantes: les paramètres
     * (type struct) puis les données (MPI_DOUBLE). */
    int peers                 = heatsim->rank_count - 1;
    MPI_Request* requests     = NULL;
    grid_params_t* all_params = NULL;
    MPI_Datatype params_type;
    int posted = 0;
    int ret    = -1;

    if (peers == 0) {
        return 0;
    }

    if (params_type_create(&params_type) < 0) {
        return -1;
    }

    requests   = malloc(2 * peers * sizeof(MPI_Request));
    all_params = malloc(peers * sizeof(grid_params_t));
    if (requests == NULL || all_params == NULL) {
        goto done;
    }

    for (int rank = 1; rank < heatsim->rank_count; rank++) {
        int coords[2];
        if (MPI_Cart_coords(heatsim->communicator, rank, 2, coords) != MPI_SUCCESS) {
            goto wait;
        }

        grid_t* grid = cart2d_get_grid(cart, coords[1], coords[0]);
        if (grid == NULL) {
            goto wait;
        }

        grid_params_t* params = &all_params[rank - 1];
        params->width         = grid->width;
        params->height        = grid->height;
        params->padding       = grid->padding;

        if (MPI_Isend(params, 1, params_type, rank, TAG_PARAMS, heatsim->communicator, &requests[posted]) !=
            MPI_SUCCESS) {
            goto wait;
        }
        posted++;

        if (MPI_Isend(grid->data, grid->width_padded * grid->height_padded, MPI_DOUBLE, rank, TAG_DATA,
                      heatsim->communicator, &requests[posted]) != MPI_SUCCESS) {
            goto wait;
        }
        posted++;
    }

    ret = 0;

wait:
    /* Toujours attendre les requêtes déjà postées avant de libérer les
     * tampons, même après une erreur. */
    if (MPI_Waitall(posted, requests, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        ret = -1;
    }

done:
    free(requests);
    free(all_params);
    MPI_Type_free(&params_type);
    return ret;
}

grid_t* heatsim_receive_grid(heatsim_t* heatsim) {
    MPI_Datatype params_type;
    grid_params_t params;
    MPI_Request request;
    grid_t* grid = NULL;

    if (params_type_create(&params_type) < 0) {
        return NULL;
    }

    if (MPI_Irecv(&params, 1, params_type, 0, TAG_PARAMS, heatsim->communicator, &request) != MPI_SUCCESS) {
        goto done;
    }
    if (MPI_Wait(&request, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        goto done;
    }

    /* Les dimensions ne sont connues qu'ici: la grille est allouée après
     * les paramètres, puis les données arrivent directement dedans. */
    grid = grid_create(params.width, params.height, params.padding);
    if (grid == NULL) {
        goto done;
    }

    if (MPI_Irecv(grid->data, grid->width_padded * grid->height_padded, MPI_DOUBLE, 0, TAG_DATA,
                  heatsim->communicator, &request) != MPI_SUCCESS ||
        MPI_Wait(&request, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        grid_destroy(grid);
        grid = NULL;
    }

done:
    MPI_Type_free(&params_type);
    return grid;
}

/* Un décalage le long d'un anneau (une dimension périodique): ce rang envoie `send_buf` à `dest` et
 * reçoit `recv_buf` de `src`, avec MPI_Send / MPI_Recv bloquants.
 *
 * Si tous les rangs envoyaient d'abord, aucun ne recevrait: interblocage dès que le message est
 * trop gros pour être copié par MPI. L'ordre dépend donc de la parité de la coordonnée le long de
 * l'anneau: les rangs pairs envoient puis reçoivent, les impairs reçoivent puis envoient. Cela
 * fonctionne aussi pour un anneau de taille impaire (le dernier rang et le rang 0 sont tous deux
 * pairs, mais le rang 0 envoie à un rang impair qui est déjà en réception, ce qui débloque la
 * chaîne) et de taille 2 (les deux voisins sont le même rang, mais les messages ont des étiquettes
 * différentes). Un anneau de taille 1 est son propre voisin: envoyer à soi-même bloquerait, la
 * bordure est donc copiée localement (MPI_Pack / MPI_Unpack, qui respectent les types dérivés). */
static int ring_shift(heatsim_t* heatsim, int coord, void* send_buf, MPI_Datatype send_type, int dest,
                      void* recv_buf, MPI_Datatype recv_type, int src, int tag) {
    MPI_Comm comm = heatsim->communicator;

    if (0) {
        int size, position = 0;
        if (MPI_Pack_size(1, send_type, comm, &size) != MPI_SUCCESS) {
            return -1;
        }

        void* packed = malloc(size);
        if (packed == NULL) {
            return -1;
        }

        int ret = -1;
        if (MPI_Pack(send_buf, 1, send_type, packed, size, &position, comm) == MPI_SUCCESS) {
            position = 0;
            if (MPI_Unpack(packed, size, &position, recv_buf, 1, recv_type, comm) == MPI_SUCCESS) {
                ret = 0;
            }
        }

        free(packed);
        return ret;
    }

    if (MPI_Send(send_buf, 1, send_type, dest, tag, comm) != MPI_SUCCESS) {
        return -1;
    }
    if (MPI_Recv(recv_buf, 1, recv_type, src, tag, comm, MPI_STATUS_IGNORE) != MPI_SUCCESS) {
        return -1;
    }
    return 0;
}

int heatsim_exchange_borders(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 1);

    /*
     * Bordure sans le rembourrage -> rembourrage du voisin:
     *
     *                            +-------------+
     *                            | x m n o p x |
     *                            | d A B C D a |
     *                            | h E F G H e |
     *                            | l I J K L i |
     *                            | p M N O P m |
     *                            | x a b c d x |
     *                            +-------------+
     *
     * Les lignes (nord/sud) sont contiguës en mémoire; les colonnes (ouest/est) sont espacées de
     * `width_padded`. Quatre décalages, un par direction.
     */
    MPI_Datatype row_type;
    MPI_Datatype column_type;
    int ret = -1;

    if (MPI_Type_contiguous(grid->width, MPI_DOUBLE, &row_type) != MPI_SUCCESS) {
        return -1;
    }
    if (MPI_Type_commit(&row_type) != MPI_SUCCESS) {
        MPI_Type_free(&row_type);
        return -1;
    }

    if (MPI_Type_vector(grid->height, 1, grid->width_padded, MPI_DOUBLE, &column_type) != MPI_SUCCESS) {
        MPI_Type_free(&row_type);
        return -1;
    }
    if (MPI_Type_commit(&column_type) != MPI_SUCCESS) {
        MPI_Type_free(&column_type);
        MPI_Type_free(&row_type);
        return -1;
    }

    int y = heatsim->coordinates[0];
    int x = heatsim->coordinates[1];

    if (ring_shift(heatsim, y, grid_get_cell(grid, 0, 0), row_type, heatsim->rank_north_peer,
                   grid_get_cell(grid, 0, grid->height), row_type, heatsim->rank_south_peer, TAG_NORTH) < 0 ||
        ring_shift(heatsim, y, grid_get_cell(grid, 0, grid->height - 1), row_type, heatsim->rank_south_peer,
                   grid_get_cell(grid, 0, -1), row_type, heatsim->rank_north_peer, TAG_SOUTH) < 0 ||
        ring_shift(heatsim, x, grid_get_cell(grid, 0, 0), column_type, heatsim->rank_west_peer,
                   grid_get_cell(grid, grid->width, 0), column_type, heatsim->rank_east_peer, TAG_WEST) < 0 ||
        ring_shift(heatsim, x, grid_get_cell(grid, grid->width - 1, 0), column_type, heatsim->rank_east_peer,
                   grid_get_cell(grid, -1, 0), column_type, heatsim->rank_west_peer, TAG_EAST) < 0) {
        goto done;
    }

    ret = 0;

done:
    MPI_Type_free(&column_type);
    MPI_Type_free(&row_type);
    return ret;
}

int heatsim_send_result(heatsim_t* heatsim, grid_t* grid) {
    assert(grid->padding == 0);

    MPI_Request request;

    if (MPI_Isend(grid->data, grid->width * grid->height, MPI_DOUBLE, 0, TAG_RESULT, heatsim->communicator,
                  &request) != MPI_SUCCESS) {
        return -1;
    }

    return MPI_Wait(&request, MPI_STATUS_IGNORE) == MPI_SUCCESS ? 0 : -1;
}

int heatsim_receive_results(heatsim_t* heatsim, cart2d_t* cart) {
    int peers = heatsim->rank_count - 1;
    MPI_Request* requests;
    int posted = 0;
    int ret    = -1;

    if (peers == 0) {
        return 0;
    }

    requests = malloc(peers * sizeof(MPI_Request));
    if (requests == NULL) {
        return -1;
    }

    for (int rank = 1; rank < heatsim->rank_count; rank++) {
        int coords[2];
        if (MPI_Cart_coords(heatsim->communicator, rank, 2, coords) != MPI_SUCCESS) {
            goto wait;
        }

        grid_t* grid = cart2d_get_grid(cart, coords[1], coords[0]);
        if (grid == NULL) {
            goto wait;
        }

        if (MPI_Irecv(grid->data, grid->width * grid->height, MPI_DOUBLE, rank, TAG_RESULT, heatsim->communicator,
                      &requests[posted]) != MPI_SUCCESS) {
            goto wait;
        }
        posted++;
    }

    ret = 0;

wait:
    if (MPI_Waitall(posted, requests, MPI_STATUSES_IGNORE) != MPI_SUCCESS) {
        ret = -1;
    }

    free(requests);
    return ret;
}
