#ifndef ATTACK_H
#define ATTACK_H

#include "board.h"

Bitboard pawn_attacks(int sq, Color side);
Bitboard knight_attacks(int sq);
Bitboard king_attacks(int sq);
Bitboard bishop_attacks(int sq, Bitboard occ);
Bitboard rook_attacks(int sq, Bitboard occ);
Bitboard queen_attacks(int sq, Bitboard occ);
void     init_attacks(void);

#endif
