<?php

declare(strict_types=1);

namespace App;

final class KDTree
{
    /** @var resource */
    private $handle;

    public function __construct(string $indexPath)
    {
        $this->handle = kdtree_load($indexPath);
    }

    /** @param array<int, float|int> $vector */
    public function fraudCount(array $vector): int
    {
        return kdtree_fraud_count($this->handle, $vector);
    }
}
