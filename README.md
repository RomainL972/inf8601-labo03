# Laboratoire 3

Simulation de chaleur

_Écrit par Gabriel-Andrew Pollo-Guilbert, idée originale de Francis Giraldeau, mis à jour par
Félix Gagnon (2026)_

Lorsque la résolution d'un problème demande beaucoup plus de ressources que ce qu'un seul
ordinateur peut fournir, il est souvent idéal de distribuer le calcul sur plusieurs machines,
communément appelé superordinateur ou une grappe de calcul de nos jours. Les simulations
scientifiques (par exemple le
[repliement des protéines](https://en.wikipedia.org/wiki/Protein_folding), la
[dynamique des fluides](https://en.wikipedia.org/wiki/Fluid_dynamics) et autres) font souvent
appel à des [superordinateurs](https://en.wikipedia.org/wiki/Supercomputer) pour accélérer le
calcul.

Ce laboratoire a pour but de vous familiariser avec la technologie
[Message Passing Interface](https://en.wikipedia.org/wiki/Message_Passing_Interface) (MPI)
développée et utilisée par et pour le domaine du calcul haute performance. Pour ce faire, vous
allez compléter la communication entre les noeuds d'une simulation de chaleur distribuée.

## Code

- `source/main.c`
  - Contient le point d'entrée du programme qui traite les arguments en ligne de commande et
    démarre la simulation de chaleur.
- `source/image.c` `include/image.h` `include/pixel.h` `source/color.c` `include/color.h`
  - Contiennent les structures et le code permettant la lecture/écriture d'images de format PNG.
- `source/grid.c` `include/grid.h`
  - Contiennent la grille de calcul utilisée par la simulation de chaque noeud.
- `source/cart.c` `include/cart.h`
  - Contiennent une structure de données permettant de diviser le problème initial en plusieurs
    grilles de calcul et de fusionner les grilles résultantes afin de créer l'image finale.
- `source/heatsim.c` `include/heatsim.h`
  - Contiennent la simulation de chaleur utilisant la méthode
    [Runge-Kutta](https://w.wiki/ZuY).
- `source/heatsim-mpi.c` (**À COMPLÉTER**)
  - Contient la communication MPI demandée de la simulation de chaleur.
- `tests/`
  - Contient des tests unitaires de `grid.c` et `cart.c`.
- `scripts/`
  - Contient les scripts CMake (`check-topology.cmake`, `run-all.cmake`) utilisés par les tests `check-*` et la cible `run-all`.

Seul le fichier marqué **À COMPLÉTER** doit être modifié.

## Algorithme

La simulation de diffusion de chaleur s'effectue sur une image en 2D. Pour paralléliser la
simulation, on divise cette image en N×M blocs (possiblement de tailles différentes). La
diffusion de chaleur à chaque itération est calculée localement pour chaque bloc. Pour chaque
noeud, l'algorithme est le suivant :

1. Initialisation du noeud MPI (**À COMPLÉTER**) — `heatsim_init`.
2. Chargement de l'image et division en sous-grilles.
   - Le rang 0 envoie les grilles aux autres rangs (**À COMPLÉTER**) — `heatsim_send_grids`.
   - Les autres rangs reçoivent leur grille du rang 0 (**À COMPLÉTER**) —
     `heatsim_receive_grid`.
3. Pour chaque itération, les noeuds calculent localement la diffusion de chaleur et échangent
   les bordures de leurs grilles.
   - Chaque rang envoie et reçoit les 4 bordures de sa grille à ses 4 voisins (**À COMPLÉTER**)
     — `heatsim_exchange_borders`.
4. Fusion des sous-grilles en une seule et enregistrement de l'image résultante.
   - Le rang 0 reçoit les grilles des autres rangs (**À COMPLÉTER**) —
     `heatsim_receive_results`.
   - Les autres rangs envoient leur grille au rang 0 (**À COMPLÉTER**) — `heatsim_send_result`.

**L'information spécifique à chaque étape est écrite en commentaire dans `heatsim-mpi.c`.**

## Spécifications

<!-- SPEC -->

De plus, les contraintes suivantes sont imposées :

- La compilation ne doit pas lancer d'avertissements.
- Le programme ne doit pas avoir de fuite de mémoire durant son exécution autre que celles
  provenant de MPI.

Le respect de ces spécifications est vérifié à la lecture du code lors de la correction ; il n'y
a pas de vérificateur automatique. La note de performance, elle, est mesurée automatiquement (voir
_Correction_).

### Environnement de développement

Il vous faut Clang, CMake, Ninja, [OpenMPI](https://www.open-mpi.org/) et libpng.

- **Linux** : `sudo apt install clang cmake ninja-build openmpi-bin libopenmpi-dev libpng-dev`
  (ou l'équivalent de votre distribution).
- **macOS** : `brew install llvm cmake ninja open-mpi libpng` et configurez avec
  `-DCMAKE_C_COMPILER=$(brew --prefix llvm)/bin/clang`.
- **Windows** : utilisez WSL (Ubuntu) ou le Dev Container ci-dessous. MS-MPI n'est pas supporté.
- **Dev Container** : sous n'importe quel système, ouvrez le dépôt dans
  [VS Code](https://code.visualstudio.com/) avec l'extension
  [Dev Containers](https://marketplace.visualstudio.com/items?itemName=ms-vscode-remote.remote-containers).
  L'image contient tous les outils et `mpirun` permet de démarrer plus de rangs que de coeurs.

### Compilation

```
$ cmake -B build -G Ninja
$ cmake --build build
```

Il n'est pas nécessaire de re-exécuter la première commande pour recompiler, seulement la
seconde. Exécutez `./build/heatsim --help` pour voir les options du programme. Le fichier
`build/compile_commands.json` est généré pour votre éditeur (clangd, etc.).

### Exécution

Pour exécuter la simulation, il faut utiliser `mpirun`. Ce programme offert par MPI permet de
démarrer plusieurs processus, localement ou à distance, chacun exécutant le même code, mais avec
un rang différent. Par exemple, pour une simulation sur 16 noeuds répartis sur une grille 4×4 :

```
$ mpirun -n 16 ./build/heatsim --dim-x 4 --dim-y 4 --input images/earth-medium.png --iterations 100
```

Seul le canal rouge de l'image d'entrée est utilisé. Si le fichier de sortie n'est pas spécifié
avec `--output`, la valeur par défaut sera le fichier d'entrée avec le suffixe `.output.png`.

**La taille de la grille (`-n`) doit être égale au nombre de noeuds (`--dim-x` × `--dim-y`).**
Si votre machine a moins de coeurs que de rangs, ajoutez `--oversubscribe` à `mpirun`.

### Commandes

Les commandes se lancent avec `ninja -C build <cible>`.

- `run-serial`, `run-mpi`
  - Exécutent la simulation sur 1 rang (1×1) et sur 3×4 rangs respectivement.
- `run-all`
  - Exécute les deux, affiche les temps et l'accélération (1000 itérations par défaut, 12000 à la
    correction, voir _Correction_). Les paramètres se changent avec
    `-DHEATSIM_IMAGE=...`, `-DHEATSIM_ITERATIONS=...`, `-DHEATSIM_DIM_X=...` et
    `-DHEATSIM_DIM_Y=...` à la configuration.
- `run-check`
  - Vérifie les six topologies d'un coup (1×1, 1×5, 3×3, 2×5, 4×4 et 8×4) : pour chacune, la
    simulation s'exécute sur 10 itérations et le SHA-256 de l'image produite doit être celui de
    `images/check<N>.checksum` (N = nombre de rangs), c'est-à-dire l'image de la solution de
    référence. Affiche une ligne `Passed` ou `Failed` par topologie. C'est la cible à lancer en
    premier : si elle échoue, la correction vous donnera 0 sans mesurer le temps.
  - Une topologie qui ne termine pas est un interblocage : le test est arrêté après 120 s et
    échoue (`Timeout`). Les topologies 1×5 (un rang est son propre voisin en X, anneau impair en
    Y) et 3×3 (anneaux impairs) sont celles qui bloquent un échange de bordures mal ordonné.
- `test` (ou `ctest --test-dir build`)
  - Exécute les tests unitaires (`test-grid`, `test-cart`) et un test par topologie
    (`check-1x1`, `check-1x5`, `check-3x3`, `check-2x5`, `check-4x4`, `check-8x4`), soit les mêmes
    vérifications que `run-check`. Pour en relancer un seul :
    `ctest --test-dir build -R check-3x3 --output-on-failure`.
- `format`
  - Utilise `clang-format` pour formatter le code source.

### Détection d'erreurs mémoire (AddressSanitizer)

Configurez le projet avec `-DENABLE_ASAN=ON`, dans un dossier séparé :

```
$ cmake -B build-asan -G Ninja -DENABLE_ASAN=ON
$ cmake --build build-asan
$ ctest --test-dir build-asan
```

## Correction

Votre `source/heatsim-mpi.c` est compilé et exécuté automatiquement dans un conteneur isolé, sur un
serveur de 64 coeurs, en deux étapes.

1. **Tests.** Les tests unitaires et la vérification des six topologies (`ctest`, voir
   _Commandes_). Un résultat incorrect ou un blocage (délai dépassé) donne la note **0**, sans
   aucune mesure de performance.
2. **Mesure.** La simulation est exécutée sur `images/earth-medium.png` pendant **12000 itérations**,
   d'abord sur un rang (1×1, environ 4 minutes), puis sur **64 rangs en grille 8×8**. Les deux
   images produites doivent être identiques, au bit près, à celles de la solution de référence ; la
   note dépend de l'accélération mesurée entre les deux exécutions.

Le temps comprend des coûts fixes qui ne diminuent pas avec le nombre de rangs (démarrage de
`mpirun`, lecture et écriture du PNG, distribution et collecte des grilles par le rang 0). Avec peu
d'itérations, ces coûts dominent et l'accélération paraît faible : c'est pourquoi la correction
utilise beaucoup d'itérations, et pourquoi `run-all` en donne un résultat plus faible par défaut
(1000 itérations, 3×4 rangs, sur votre machine). Pour mesurer comme la correction, si votre machine
a assez de coeurs :

```
$ cmake -B build -G Ninja -DHEATSIM_ITERATIONS=12000 -DHEATSIM_DIM_X=8 -DHEATSIM_DIM_Y=8
$ ninja -C build run-all
```

Sur votre machine, l'accélération dépend de son nombre de coeurs ; elle ne sert qu'à comparer vos
versions entre elles.

## Remise

Une fois votre travail commité, générez directement l'archive de remise avec `git archive`. Seul
`source/heatsim-mpi.c` est à remettre :

```
$ git archive --format=zip --output=matricule1_matricule2.zip HEAD -- source/heatsim-mpi.c
```
