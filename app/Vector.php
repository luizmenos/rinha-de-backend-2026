<?php

declare(strict_types=1);

namespace App;

use DateTime;
use DateTimeImmutable;
use DateTimeZone;

final class Vector
{
    public const array MCC_RISK = [
        "5411" =>  0.15,
        "5812" =>  0.30,
        "5912" =>  0.20,
        "5944" =>  0.45,
        "7801" =>  0.80,
        "7802" =>  0.75,
        "7995" =>  0.85,
        "4511" =>  0.35,
        "5311" =>  0.25,
        "5999" =>  0.50
    ];

    /**
     * @var array{
     *     max_amount: int,
     *     max_installments: int,
     *     amount_vs_avg_ratio: int,
     *     max_minutes: int,
     *     max_km: int,
     *     max_tx_count_24h: int,
     *     max_merchant_avg_amount: int
     * }
     */
    public const array NORMALIZATION_VALUES = [
        "max_amount" => 10000,
        "max_installments" =>  12,
        "amount_vs_avg_ratio" =>  10,
        "max_minutes" =>  1440,
        "max_km" =>  1000,
        "max_tx_count_24h" =>  20,
        "max_merchant_avg_amount" =>  10000,
    ];

    /**
     * Transform a transaction payload into the ordered 14-dimensional vector
     * expected by the KD-tree.
     *
     * The input is a transaction object from `resources/example_payload.json`:
     *
     * @param array{
     *     id: string,
     *     transaction: array{amount: int|float, installments: int, requested_at: string},
     *     customer: array{avg_amount: int|float, tx_count_24h: int, known_merchants: list<string>},
     *     merchant: array{id: string, mcc: string, avg_amount: int|float},
     *     terminal: array{is_online: bool, card_present: bool, km_from_home: int|float},
     *     last_transaction: null|array{timestamp: string, km_from_current: int|float}
     * } $body
     *
     * @return array{
     *     0: float, 1: float, 2: float, 3: float, 4: float, 5: float, 6: float,
     *     7: float, 8: float, 9: float, 10: float, 11: float, 12: float, 13: float
     * } Values in this order: amount, installments, amount_vs_avg, hour_of_day,
     * day_of_week, minutes_since_last_tx, km_from_last_tx, km_from_home,
     * tx_count_24h, is_online, card_present, unknown_merchant, mcc_risk,
     * merchant_avg_amount. Normalized numeric values are rounded to four decimals.
     */
    public function vectorize(array $body): array
    {
        $transaction = $body["transaction"];
        $customer = $body["customer"];
        $merchant = $body["merchant"];
        $terminal = $body["terminal"];
        $lastTransaction = $body["last_transaction"];

        $requestedAt = $transaction['requested_at'];
        $requestedAtYear = (int) substr($requestedAt, 0, 4);
        $requestedAtMonth = (int) substr($requestedAt, 5, 2);
        $requestedAtDay = (int) substr($requestedAt, 8, 2);
        $requestedAtHour = (int) substr($requestedAt, 11, 2);
        $requestedAtWeekDay = $this->weekDay($requestedAtYear, $requestedAtMonth, $requestedAtDay);

        $kmFromCurrent = null;
        $minutesBetween = null;

        if ($lastTransaction) {
            $lastTransactionTimestamp = $lastTransaction['timestamp'];
            $kmFromCurrent = (float) $lastTransaction['km_from_current'];
            $minutesBetween = $this->minutesBetween(new Datetime($requestedAt), new DateTime("{$lastTransactionTimestamp}"));
        }

        $vector = [];

        $vector[] = $this->clamp($transaction["amount"] / self::NORMALIZATION_VALUES["max_amount"]);
        $vector[] = $this->clamp($transaction["installments"] / self::NORMALIZATION_VALUES["max_installments"]);
        $vector[] = $this->clamp(($transaction["amount"] / $customer['avg_amount']) / self::NORMALIZATION_VALUES["amount_vs_avg_ratio"]);
        $vector[] = $requestedAtHour / 23.0;
        $vector[] = $requestedAtWeekDay / 6;
        $vector[] = $minutesBetween ? $this->clamp($minutesBetween / self::NORMALIZATION_VALUES["max_minutes"]) : -1;
        $vector[] = $kmFromCurrent ? $this->clamp($kmFromCurrent / self::NORMALIZATION_VALUES["max_km"]) : -1;
        $vector[] = $this->clamp($terminal['km_from_home'] / self::NORMALIZATION_VALUES['max_km']);
        $vector[] = $this->clamp($customer['tx_count_24h'] / self::NORMALIZATION_VALUES['max_tx_count_24h']);
        $vector[] = intval($terminal['is_online']);
        $vector[] = intval($terminal['card_present']);
        $vector[] = intval(!in_array($merchant['id'], $customer['known_merchants']));
        $vector[] = self::MCC_RISK[$merchant['mcc']] ?? 0.5;
        $vector[] = $this->clamp($merchant['avg_amount'] / self::NORMALIZATION_VALUES['max_merchant_avg_amount']);

        return $vector;
    }

    private function clamp(float $value): float
    {
        if ($value < 0) {
            $value = 0;
        }
        if ($value > 1) {
            $value = 0;
        }

        return $this->round($value);
    }

    private function round(float $value): float
    {
        return round($value, 4);
    }

    private function weekDay(int $year, int $month, int $day): int
    {
        return (int) (new DateTimeImmutable(
            sprintf('%04d-%02d-%02d', $year, $month, $day),
            new DateTimeZone('UTC')
        ))->format('N') - 1;
    }

    private function minutesBetween(DateTime $from, DateTime $to): int
    {
        $since = $from->diff($to);
        $minutes = $since->days * 24 * 60;
        $minutes += $since->h * 60;
        $minutes += $since->i;
        return $minutes;
    }
}
