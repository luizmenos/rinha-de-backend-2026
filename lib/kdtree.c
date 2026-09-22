#include <stdio.h>
#include <stdlib.h>
#include <float.h>

#define DIMENSIONS 14 // 14 dimensoes conforme docs do desafio https://github.com/zanfranceschi/rinha-de-backend-2026/tree/main/docs/br

int sort_axis = 0;

typedef struct Node
{
    double point[DIMENSIONS];
    struct Node *left;
    struct Node *right;
} Node;

Node *create_node(const double *point)
{
    Node *node = (Node *)malloc(sizeof(Node));

    if (node == NULL)
        return NULL;

    for (int i = 0; i < DIMENSIONS; i++)
    {
        node->point[i] = point[i];
    }

    node->left = NULL;
    node->right = NULL;
    return node;
}

typedef struct KDTree
{
    Node *root;
} KDTree;

int compare_points(const void *a, const void *b)
{
    const double *point_a = (const double *)a;
    const double *point_b = (const double *)b;

    if (point_a[sort_axis] < point_b[sort_axis])
        return -1;
    if (point_a[sort_axis] > point_b[sort_axis])
        return 1;
    return 0;
}

Node *build_tree(double points[][DIMENSIONS], int num_points, int depth)
{
    if (num_points == 0)
        return NULL;

    sort_axis = depth % DIMENSIONS;

    qsort(points, num_points, sizeof(points[0]), compare_points);

    int mid = num_points / 2;

    Node *root = create_node(points[mid]);
    if (!root)
        return NULL;

    root->left = build_tree(points, mid, depth + 1);
    root->right = build_tree(&points[mid + 1], num_points - mid - 1, depth + 1);

    return root;
}

double squared_distance(const double *p1, const double *p2)
{
    double sum = 0.0;
    for (int i = 0; i < DIMENSIONS; i++)
    {
        double diff = p1[i] - p2[i];
        sum += diff * diff;
    }
    return sum;
}

void search_nearest_recursive(Node *root, const double *target, int depth, Node **best_node, double *best_distance)
{
    if (root == NULL)
        return;

    double current_distance = squared_distance(root->point, target);

    if (current_distance < *best_distance)
    {
        *best_distance = current_distance;
        *best_node = root;
    }

    int axis = depth % DIMENSIONS;

    Node *near_child = (target[axis] < root->point[axis]) ? root->left : root->right;
    Node *far_child = (target[axis] < root->point[axis]) ? root->right : root->left;

    search_nearest_recursive(near_child, target, depth + 1, best_node, best_distance);

    double axis_diff = target[axis] - root->point[axis];
    double squared_diff = axis_diff * axis_diff;

    if (squared_diff < *best_distance)
    {
        search_nearest_recursive(far_child, target, depth + 1, best_node, best_distance);
    }
}

Node *search_nearest_neighbor(Node *root, const double *target)
{
    if (root == NULL)
        return NULL;

    Node *best_node = NULL;
    double best_distance = DBL_MAX;

    search_nearest_recursive(root, target, 0, &best_node, &best_distance);

    return best_node;
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