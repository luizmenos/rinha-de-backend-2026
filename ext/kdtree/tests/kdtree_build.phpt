--TEST--
kdtree_build creates a tree with labeled references
--FILE--
<?php
$references = [
    ['vector' => array_fill(0, 14, 0.0), 'label' => 'legit'],
    ['vector' => array_fill(0, 14, 1.0), 'label' => 'fraud'],
];

$tree = kdtree_build($references, 2, 0);
var_dump(is_resource($tree));
?>
--EXPECT--
bool(true)