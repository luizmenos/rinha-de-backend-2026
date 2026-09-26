#ifndef KDTREE_H
#define KDTREE_H

#define DIMENSIONS 14

typedef struct Node
{
    double point[DIMENSIONS];
    struct Node *left;
    struct Node *right;
} Node;

Node *build_tree(double points[][DIMENSIONS], int num_points, int depth);

void destroy_tree(Node *root);

#endif