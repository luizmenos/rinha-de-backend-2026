#include <stdio.h>
#include <stdlib.h>

#define K 14 // 14 dimensoes conforme docs do desafio https://github.com/zanfranceschi/rinha-de-backend-2026/tree/main/docs/br

int current_axis = 0;

typedef struct Node
{
    double point[K];
    struct Node *left;
    struct Node *right;
} Node;

Node *create_node(const double *pt)
{
    Node *new = (Node *)malloc(sizeof(Node));

    if (new == NULL)
        return NULL;

    for (int i = 0; i < K; i++)
    {
        new->point[i] = pt[i];
    }

    new->left = NULL;
    new->right = NULL;
    return new;
}

typedef struct KDTree
{
    Node *root;
} KDTree;

int point_comparison(const void *a, const void *b)
{
    const double *pointA = (const double *)a;
    const double *pointB = (const double *)b;

    if (pointA[current_axis] < pointB[current_axis])
        return -1;
    if (pointA[current_axis] > pointB[current_axis])
        return 1;
    return 0;
}

Node *build_tree(double points[][K], int num_points, int depth)
{
    if (num_points == 0)
        return NULL;

    current_axis = depth % K;

    qsort(points, num_points, sizeof(points[0]), point_comparison);

    int mid = num_points / 2;

    Node *root = create_node(points[mid]);
    if (!root)
        return NULL;

    root->left = build_tree(points, mid, depth + 1);
    root->right = build_tree(&points[mid + 1], num_points - mid - 1, depth + 1);

    return root;
}

void destroy_tree(Node *root)
{
    if (!root)
        return;

    destroy_tree(root->left);
    destroy_tree(root->right);

    free(root);

    return;
}