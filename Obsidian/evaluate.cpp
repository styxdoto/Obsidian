#include "evaluate.h"

#include <iostream>

namespace Eval {

  Score evaluate(Position& pos, NNUE::Accumulator& accumulator) {

    Score score = NNUE::evaluate(pos, accumulator);

    int phase =  3 * BitCount(pos.pieces(KNIGHT))
               + 3 * BitCount(pos.pieces(BISHOP))
               + 5 * BitCount(pos.pieces(ROOK))
               + 10 * BitCount(pos.pieces(QUEEN));    

    score = score * (206 + phase) / 256;           

    // Scale down as 50 move rule approaches
    score = score * (200 - pos.halfMoveClock) / 200;

    // Make sure the evaluation does not mix with guaranteed win/loss scores
    score = std::clamp(score, SCORE_TB_LOSS_IN_MAX_PLY + 1, SCORE_TB_WIN_IN_MAX_PLY - 1);

    return score;
  }

#undef pos
}