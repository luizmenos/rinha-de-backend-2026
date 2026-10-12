#ifndef KDTREE_H
#define KDTREE_H

#define DIMENSIONS 14
#define K_NEIGHBORS 5

/* 0 = legit, 1 = fraud */
typedef struct Node
{
    double point[DIMENSIONS];
    unsigned char is_fraud;
    struct Node *left;
    struct Node *right;
} Node;

typedef struct KDTree
{
    Node *root;
} KDTree;

typedef struct Candidate
{
    Node *node;
    double distance;
} Candidate;

typedef struct Reference
{
    double point[DIMENSIONS];
    unsigned char is_fraud;
} Reference;

Node *build_tree(Reference *references, int num_points, int depth);
int search_5_nearest_neighbors(Node *root, const double *target, Candidate best[K_NEIGHBORS]);
void destroy_tree(Node *root);

#endif