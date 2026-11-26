import math
import unittest

import elo


class EloTests(unittest.TestCase):
    def test_even_score_is_zero(self):
        summary = elo.summarize(10, 20, 10)
        self.assertAlmostEqual(summary.elo, 0.0, places=6)
        self.assertAlmostEqual(summary.score, 0.5)

    def test_known_value(self):
        self.assertAlmostEqual(elo.elo_from_score(0.75), 400 * math.log10(3), places=6)
        self.assertAlmostEqual(elo.elo_from_score(0.75), 190.85, places=2)

    def test_score_and_elo_are_inverse(self):
        for value in (-300, -50, 0, 120, 400):
            self.assertAlmostEqual(elo.elo_from_score(elo.score_from_elo(value)), value, places=6)

    def test_swapping_sides_negates_the_difference(self):
        a = elo.summarize(38, 6, 16)
        b = elo.summarize(16, 6, 38)
        self.assertAlmostEqual(a.elo, -b.elo, places=6)
        self.assertAlmostEqual(a.elo_low, -b.elo_high, places=6)
        self.assertAlmostEqual(a.elo_high, -b.elo_low, places=6)

    def test_matches_the_commit_12_result(self):
        summary = elo.summarize(38, 6, 16)
        self.assertAlmostEqual(summary.elo, 134, delta=1)
        self.assertAlmostEqual(summary.elo_low, 51, delta=1)
        self.assertAlmostEqual(summary.elo_high, 234, delta=1)

    def test_interval_shrinks_with_more_games(self):
        small = elo.summarize(15, 3, 8)
        large = elo.summarize(150, 30, 80)
        self.assertAlmostEqual(small.elo, large.elo, delta=1)
        self.assertLess(large.elo_high - large.elo_low, small.elo_high - small.elo_low)

    def test_interval_contains_the_estimate(self):
        for result in ((5, 5, 5), (30, 10, 20), (1, 0, 9), (12, 0, 1)):
            summary = elo.summarize(*result)
            self.assertLessEqual(summary.elo_low, summary.elo)
            self.assertLessEqual(summary.elo, summary.elo_high)

    def test_draws_narrow_the_interval(self):
        decisive = elo.summarize(30, 0, 10)
        drawish = elo.summarize(20, 20, 0)
        self.assertAlmostEqual(decisive.score, drawish.score)
        self.assertLess(drawish.elo_high - drawish.elo_low, decisive.elo_high - decisive.elo_low)

    def test_shutout_is_flagged_and_finite(self):
        summary = elo.summarize(20, 0, 0)
        self.assertTrue(summary.bounded)
        self.assertTrue(math.isfinite(summary.elo))
        self.assertGreater(summary.elo, 400)
        shutout_against = elo.summarize(0, 0, 20)
        self.assertTrue(shutout_against.bounded)
        self.assertAlmostEqual(summary.elo, -shutout_against.elo, places=6)

    def test_likelihood_of_superiority(self):
        self.assertAlmostEqual(elo.summarize(10, 5, 10).los, 0.5)
        self.assertGreater(elo.summarize(30, 0, 10).los, 0.99)
        self.assertLess(elo.summarize(10, 0, 30).los, 0.01)
        self.assertEqual(elo.likelihood_of_superiority(0, 0), 0.5)

    def test_rejects_bad_input(self):
        with self.assertRaises(ValueError):
            elo.summarize(0, 0, 0)
        with self.assertRaises(ValueError):
            elo.summarize(-1, 2, 3)

    def test_small_samples_are_flagged(self):
        self.assertIn("unreliable", elo.format_summary(elo.summarize(3, 1, 2)))
        self.assertNotIn("unreliable", elo.format_summary(elo.summarize(38, 6, 16)))

    def test_format_mentions_the_counts(self):
        text = elo.format_summary(elo.summarize(38, 6, 16), "Arivu", "Old")
        self.assertIn("+38 =6 -16", text)
        self.assertIn("Arivu vs Old", text)


if __name__ == "__main__":
    unittest.main()
