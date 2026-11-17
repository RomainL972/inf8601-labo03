# Spécifications de l'implémentation

- Les paramètres `width`, `height` et `padding` d'une grille doivent être envoyés en une seule requête, de type défini avec `MPI_Type_create_struct`.
- L'envoi et la réception des grilles initiales (paramètres, puis données `data`) doivent être non bloquants: `MPI_Isend` et `MPI_Irecv`.
- Les données (`data`) d'une grille sont envoyées en une seule requête de type `MPI_DOUBLE`.
- L'échange des bordures doit être effectué avec `MPI_Send` et `MPI_Recv` (bloquants; ni `MPI_Sendrecv` ni les appels non bloquants). Il ne doit jamais se bloquer, quelle que soit la topologie : grille 1×N ou N×1 (un rang est son propre voisin), dimension de taille 2 (le voisin nord est aussi le voisin sud) ou de taille impaire.
- Les bordures nord et sud doivent être de type défini avec `MPI_Type_contiguous`.
- Les bordures est et ouest doivent être de type défini avec `MPI_Type_vector`.
- L'envoi et la réception de la grille finale doivent être non bloquants: `MPI_Isend` et `MPI_Irecv`, de type `MPI_DOUBLE`.
- Tous les types dérivés créés doivent être libérés avec `MPI_Type_free`.
