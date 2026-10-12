--TEST--
kdtree_search returns the five nearest labels in distance order
--FILE--
<?php
$reference = static function (float $x, string $label): array {
    $vector = array_fill(0, 14, 0.0);
    $vector[0] = $x;
    return ['vector' => $vector, 'label' => $label];
};

$references = [
    $reference(0.0, 'legit'),
    $reference(0.1, 'fraud'),
    $reference(0.2, 'legit'),
    $reference(0.3, 'fraud'),
    $reference(0.4, 'fraud'),
    $reference(0.5, 'legit'),
];

$tree = kdtree_build($references, count($references), 0);
$neighbors = kdtree_search($tree, array_fill(0, 14, 0.0));

echo count($neighbors), "\n";
echo implode(',', array_column($neighbors, 'label')), "\n";
echo round($neighbors[4]['distance_squared'], 2), "\n";
?>
--EXPECT--
5
legit,fraud,legit,fraud,fraud
0.16
