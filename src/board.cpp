#include "board.h"

#include <cstddef>
#include <iostream>
#include <sstream>
#include <string>

#include "bitboard.h"

namespace {
const std::string PIECE_CHARS = "PNBRQKpnbrqk";
}

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