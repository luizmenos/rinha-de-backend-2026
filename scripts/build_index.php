<?php

declare(strict_types=1);

/**
 * Convert references.json.gz (a JSON array) into a compact binary file.
 *
 * File format, little-endian:
 *   8 bytes  magic: RINHAIDX
 *   uint32   version (1)
 *   uint32   number of references
 *   repeated records:
 *     int16[14] vector coordinates multiplied by 10,000
 *     uint8    label (0 = legit, 1 = fraud)
 */

$inputPath = $argv[1] ?? __DIR__ . '/../resources/references.json.gz';
$outputPath = $argv[2] ?? __DIR__ . '/../resources/references.idx';

if (!is_file($inputPath)) {
    fwrite(STDERR, "Input file not found: {$inputPath}\n");
    exit(1);
}

$input = gzopen($inputPath, 'rb');
if ($input === false) {
    fwrite(STDERR, "Could not open gzip input: {$inputPath}\n");
    exit(1);
}

$output = fopen($outputPath, 'wb+');
if ($output === false) {
    gzclose($input);
    fwrite(STDERR, "Could not create output: {$outputPath}\n");
    exit(1);
}

fwrite($output, 'RINHAIDX' . pack('V2', 1, 0));

$count = 0;
$binaryBuffer = '';

$writeRecord = static function (string $json) use ($output, &$count, &$binaryBuffer): void {
    if (preg_match('/"vector"\s*:\s*\[([^\]]+)\]\s*,\s*"label"\s*:\s*"(fraud|legit)"/', $json, $matches) !== 1) {
        throw new RuntimeException('Each reference must contain a numeric 14-value vector and a fraud/legit label.');
    }
    $values = explode(',', $matches[1]);
    if (count($values) !== 14) {
        throw new RuntimeException('Each reference vector must contain exactly 14 values.');
    }

    $label = match ($matches[2]) {
        'legit' => 0,
        'fraud' => 1,
    };

    $coordinates = [];
    foreach ($values as $value) {
        $value = trim($value);
        if (!is_numeric($value)) {
            throw new RuntimeException('Vector coordinates must be numbers.');
        }
        $quantized = (int) round((float)$value * 10_000);
        if ($quantized < -32_768 || $quantized > 32_767) {
            throw new RuntimeException('Quantized vector coordinate exceeds int16 range.');
        }
        $coordinates[] = $quantized;
    }

    $binaryBuffer .= pack('v*', ...$coordinates) . chr($label);
    if (strlen($binaryBuffer) >= 1_000_000) {
        $bytes = fwrite($output, $binaryBuffer);
        if ($bytes !== strlen($binaryBuffer)) {
            throw new RuntimeException('Failed to write index data.');
        }
        $binaryBuffer = '';
    }
    $count++;
    if ($count % 100_000 === 0) {
        fwrite(STDERR, sprintf("Packed %d references...\n", $count));
    }
};

try {
    $buffer = '';
    while (!gzeof($input)) {
        $chunk = gzread($input, 1_048_576);
        if ($chunk === false) {
            throw new RuntimeException('Failed while reading gzip input.');
        }
        $buffer .= $chunk;

        while (($separator = strpos($buffer, '},{')) !== false) {
            $json = ltrim(substr($buffer, 0, $separator + 1), " \t\r\n,[");
            $writeRecord($json);
            $buffer = substr($buffer, $separator + 2);
        }
    }

    $lastJson = rtrim(ltrim($buffer, " \t\r\n,["), " \t\r\n]");
    if ($lastJson !== '') {
        $writeRecord($lastJson);
    }
    if ($count === 0) {
        throw new RuntimeException('Input JSON contained no references.');
    }

    if ($binaryBuffer !== '') {
        $bytes = fwrite($output, $binaryBuffer);
        if ($bytes !== strlen($binaryBuffer)) {
            throw new RuntimeException('Failed to write final index data.');
        }
    }

    if (fseek($output, 12) !== 0 || fwrite($output, pack('V', $count)) !== 4) {
        throw new RuntimeException('Failed to write index header.');
    }
    fflush($output);
} catch (Throwable $error) {
    fclose($output);
    gzclose($input);
    @unlink($outputPath);
    fwrite(STDERR, "Index build failed: {$error->getMessage()}\n");
    exit(1);
}

fclose($output);
gzclose($input);
fwrite(STDOUT, sprintf("Wrote %d references to %s\n", $count, $outputPath));
