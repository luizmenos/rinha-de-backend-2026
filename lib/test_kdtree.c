#include <assert.h>
#include <math.h>

/* Temporario: inclui a implementacao enquanto nao temos um kdtree.h.
 * Compile apenas este arquivo, sem passar kdtree.c novamente ao compilador.
 */
#include "kdtree.c"

static void check_distance(Node *tree, const double *target, double expected)
{
    Node *nearest = search_nearest_neighbor(tree, target);
    assert(nearest != NULL);
    double actual = squared_distance(nearest->point, target);
    assert(fabs(actual - expected) < 1e-9);
}

int main(void)
{
    /* Coordenadas nao especificadas sao inicializadas com zero. */
    double points[][K] = {{1}, {5}, {9}};
    double target[K] = {6};
    Node *tree = build_tree(points, 3, 0);
    check_distance(tree, target, 1.0);
    Node *nearest = search_nearest_neighbor(tree, target);
    assert(nearest->point[0] == 5.0);
    printf("OK: consulta 6 encontrou %.0f, distancia quadrada = %.0f\n",
           nearest->point[0], squared_distance(nearest->point, target));

    double exact[K] = {9};
    check_distance(tree, exact, 0.0);
    destroy_tree(tree);
    puts("OK: ponto exato");

    tree = build_tree(NULL, 0, 0);
    assert(search_nearest_neighbor(tree, target) == NULL);
    destroy_tree(tree);
    puts("OK: arvore vazia");

    double single[][K] = {{2, 3}};
    double query[K] = {5, 7};
    tree = build_tree(single, 1, 0);
    check_distance(tree, query, 25.0);
    destroy_tree(tree);
    puts("OK: ponto unico e distancia em duas dimensoes");

    /* Raiz (0,100): a consulta desce a esquerda, mas o melhor esta a direita. */
    double crossing[][K] = {{-2, 100}, {0, 100}, {1, 0}};
    double across[K] = {-1, 0};
    tree = build_tree(crossing, 3, 0);
    check_distance(tree, across, 4.0);
    destroy_tree(tree);
    puts("OK: busca precisa visitar o outro lado");

    /* Compara com varredura completa, calculando a referencia independentemente. */
    double cloud[64][K];
    for (int i = 0; i < 64; i++)
        for (int j = 0; j < K; j++)
            cloud[i][j] = ((i * 17 + j * 13 + i * j * 7) % 101) - 50;

    tree = build_tree(cloud, 64, 0);
    for (int q = 0; q < 100; q++)
    {
        double probe[K];
        for (int j = 0; j < K; j++)
            probe[j] = ((q * 23 + j * 19 + q * j * 3) % 127) - 63;

        double expected = DBL_MAX;
        for (int i = 0; i < 64; i++)
        {
            double distance = 0.0;
            for (int j = 0; j < K; j++)
            {
                double delta = cloud[i][j] - probe[j];
                distance += delta * delta;
            }
            if (distance < expected)
                expected = distance;
        }
        check_distance(tree, probe, expected);
    }
    destroy_tree(tree);
    puts("OK: 100 consultas em 14 dimensoes comparadas com busca completa");
    puts("Todos os testes passaram.");
    return 0;
}
