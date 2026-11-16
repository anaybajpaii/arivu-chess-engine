#include "board.h"

#include <array>
#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"

namespace {

const std::string PIECE_CHARS = "PNBRQKpnbrqk";

Color opposite(Color c) { return c == WHITE ? BLACK : WHITE; }

const std::array<int, 64> CASTLE_MASK = [] {
    std::array<int, 64> mask;
    mask.fill(WHITE_OO | WHITE_OOO | BLACK_OO | BLACK_OOO);
    mask[0] &= ~WHITE_OOO;
    mask[4] &= ~(WHITE_OO | WHITE_OOO);
    mask[7] &= ~WHITE_OO;
    mask[56] &= ~BLACK_OOO;
    mask[60] &= ~(BLACK_OO | BLACK_OOO);
    mask[63] &= ~BLACK_OO;
    return mask;
}();

}  // namespace

Board::Board() { clear(); }

void Board::clear() {
    pieces.fill(0);
    occupancy.fill(0);
    occupied = 0;
    mailbox.fill(NO_PIECE);
    side_to_move = WHITE;
    castling_rights = 0;
    ep_square = NO_SQUARE;
    halfmove_clock = 0;
    fullmove_number = 1;
}

void Board::put_piece(Piece p, Square s) {
    Bitboard bit = square_bb(s);
    pieces[p] |= bit;
    occupancy[color_of(p)] |= bit;
    occupied |= bit;
    mailbox[s] = p;
}

void Board::remove_piece(Square s) {
    Piece p = mailbox[s];
    Bitboard bit = square_bb(s);
    pieces[p] &= ~bit;
    occupancy[color_of(p)] &= ~bit;
    occupied &= ~bit;
    mailbox[s] = NO_PIECE;
}

void Board::move_piece(Square from, Square to) {
    Piece p = mailbox[from];
    remove_piece(from);
    put_piece(p, to);
}

Undo Board::make_move(Move m) {
    Undo u;
    u.castling_rights = castling_rights;
    u.ep_square = ep_square;
    u.halfmove_clock = halfmove_clock;

    Color us = side_to_move;
    Square from = m.from();
    Square to = m.to();
    int flags = m.flags();
    bool pawn_move = type_of(mailbox[from]) == PAWN;

    if (flags == EP_CAPTURE) {
        Square victim = (us == WHITE) ? to - 8 : to + 8;
        u.captured = mailbox[victim];
        remove_piece(victim);
    } else if (m.is_capture()) {
        u.captured = mailbox[to];
        remove_piece(to);
    }

    move_piece(from, to);

    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, m.promotion_type()), to);
    } else if (flags == KING_CASTLE) {
        move_piece(to + 1, to - 1);
    } else if (flags == QUEEN_CASTLE) {
        move_piece(to - 2, to + 1);
    }

    castling_rights &= CASTLE_MASK[from] & CASTLE_MASK[to];
    ep_square = (flags == DOUBLE_PUSH) ? (from + to) / 2 : NO_SQUARE;
    halfmove_clock = (pawn_move || u.captured != NO_PIECE) ? 0 : halfmove_clock + 1;
    if (us == BLACK) ++fullmove_number;
    side_to_move = opposite(us);
    return u;
}

void Board::unmake_move(Move m, const Undo& u) {
    Color us = opposite(side_to_move);
    side_to_move = us;
    if (us == BLACK) --fullmove_number;
    castling_rights = u.castling_rights;
    ep_square = u.ep_square;
    halfmove_clock = u.halfmove_clock;

    Square from = m.from();
    Square to = m.to();
    int flags = m.flags();

    if (m.is_promotion()) {
        remove_piece(to);
        put_piece(make_piece(us, PAWN), from);
    } else {
        move_piece(to, from);
    }

    if (flags == KING_CASTLE) {
        move_piece(to - 1, to + 1);
    } else if (flags == QUEEN_CASTLE) {
        move_piece(to + 1, to - 2);
    }

    if (flags == EP_CAPTURE) {
        Square victim = (us == WHITE) ? to - 8 : to + 8;
        put_piece(u.captured, victim);
    } else if (m.is_capture()) {
        put_piece(u.captured, to);
    }
}

bool Board::set_fen(const std::string& fen) {
    clear();
    if (!parse_fen(fen)) {
        clear();
        return false;
    }
    return true;
}

bool Board::parse_fen(const std::string& fen) {
    std::istringstream ss(fen);
    std::string placement, side, castling, ep;
    if (!(ss >> placement >> side >> castling >> ep)) return false;

    int rank = 7;
    int file = 0;
    for (char c : placement) {
        if (c == '/') {
            if (file != 8 || rank == 0) return false;
            --rank;
            file = 0;
        } else if (c >= '1' && c <= '8') {
            file += c - '0';
            if (file > 8) return false;
        } else {
            std::size_t idx = PIECE_CHARS.find(c);
            if (idx == std::string::npos || file > 7) return false;
            put_piece(static_cast<Piece>(idx), make_square(file, rank));
            ++file;
        }
    }
    if (rank != 0 || file != 8) return false;

    if (side == "w") {
        side_to_move = WHITE;
    } else if (side == "b") {
        side_to_move = BLACK;
    } else {
        return false;
    }

    if (castling != "-") {
        for (char c : castling) {
            switch (c) {
                case 'K': castling_rights |= WHITE_OO; break;
                case 'Q': castling_rights |= WHITE_OOO; break;
                case 'k': castling_rights |= BLACK_OO; break;
                case 'q': castling_rights |= BLACK_OOO; break;
                default: return false;
            }
        }
    }

    if (ep == "-") {
        ep_square = NO_SQUARE;
    } else {
        if (ep.size() != 2 || ep[0] < 'a' || ep[0] > 'h' || ep[1] < '1' || ep[1] > '8') return false;
        ep_square = make_square(ep[0] - 'a', ep[1] - '1');
    }

    int halfmove = 0;
    int fullmove = 1;
    if (ss >> halfmove) {
        if (!(ss >> fullmove)) fullmove = 1;
    } else {
        halfmove = 0;
    }
    halfmove_clock = halfmove;
    fullmove_number = fullmove;

    if (popcount(pieces[W_KING]) != 1 || popcount(pieces[B_KING]) != 1) return false;
    return true;
}

std::string Board::to_fen() const {
    std::string fen;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            Piece p = mailbox[make_square(file, rank)];
            if (p == NO_PIECE) {
                ++empty;
            } else {
                if (empty > 0) {
                    fen += static_cast<char>('0' + empty);
                    empty = 0;
                }
                fen += PIECE_CHARS[p];
            }
        }
        if (empty > 0) fen += static_cast<char>('0' + empty);
        if (rank > 0) fen += '/';
    }

    fen += (side_to_move == WHITE) ? " w " : " b ";

    if (castling_rights == 0) {
        fen += '-';
    } else {
        if (castling_rights & WHITE_OO) fen += 'K';
        if (castling_rights & WHITE_OOO) fen += 'Q';
        if (castling_rights & BLACK_OO) fen += 'k';
        if (castling_rights & BLACK_OOO) fen += 'q';
    }

    fen += ' ';
    fen += (ep_square == NO_SQUARE) ? std::string("-") : square_name(ep_square);
    fen += ' ' + std::to_string(halfmove_clock) + ' ' + std::to_string(fullmove_number);
    return fen;
}

void Board::print() const {
    for (int rank = 7; rank >= 0; --rank) {
        std::cout << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            Piece p = mailbox[make_square(file, rank)];
            std::cout << (p == NO_PIECE ? '.' : PIECE_CHARS[p]) << ' ';
        }
        std::cout << "\n";
    }
    std::cout << "   a b c d e f g h\n";
    std::cout << "fen: " << to_fen() << "\n";
}