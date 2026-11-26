import unittest

import chess

import match
from openings import OPENINGS


class OpeningTests(unittest.TestCase):
    def test_every_opening_is_legal(self):
        for name, line in OPENINGS:
            board = match.opening_board(line)
            self.assertFalse(board.is_game_over(), name)
            self.assertEqual(len(board.move_stack), 8, name)

    def test_openings_are_distinct(self):
        positions = {match.opening_board(line).board_fen() for _, line in OPENINGS}
        self.assertEqual(len(positions), len(OPENINGS))
        names = [name for name, _ in OPENINGS]
        self.assertEqual(len(set(names)), len(names))

    def test_pairs_share_an_opening_and_swap_colours(self):
        count = len(OPENINGS)
        for pair in range(count * 2):
            first = match.opening_for_game(pair * 2, count)
            second = match.opening_for_game(pair * 2 + 1, count)
            self.assertEqual(first, second)
        self.assertEqual(match.opening_for_game(0, count), 0)
        self.assertEqual(match.opening_for_game(2, count), 1)
        self.assertEqual(match.opening_for_game(count * 2, count), 0)


class ParsingTests(unittest.TestCase):
    def test_option_values(self):
        self.assertEqual(match.parse_option_value("true"), True)
        self.assertEqual(match.parse_option_value("False"), False)
        self.assertEqual(match.parse_option_value("1500"), 1500)
        self.assertEqual(match.parse_option_value("nn-file.nnue"), "nn-file.nnue")

    def test_options_dictionary(self):
        parsed = match.parse_options(["UCI_LimitStrength=true", "UCI_Elo=1500", "Hash=32"])
        self.assertEqual(parsed, {"UCI_LimitStrength": True, "UCI_Elo": 1500, "Hash": 32})
        with self.assertRaises(ValueError):
            match.parse_options(["Hash"])

    def test_time_control(self):
        self.assertEqual(match.parse_time_control("10+0.1"), (10.0, 0.1))
        self.assertEqual(match.parse_time_control("60"), (60.0, 0.0))


if __name__ == "__main__":
    unittest.main()
