#pragma once

#include "bitboard.h"
#include "nnue.h"
#include "types.h"
#include "zobrist.h"

using namespace std;

/// <summary>
/// Called once at engine initialization
/// </summary>
void positionInit();

__declspec(align(32)) struct Position {
  Color sideToMove;
  Square epSquare;
  CastlingRights castlingRights;

  Bitboard byColorBB[COLOR_NB];
  Bitboard byPieceBB[PIECE_TYPE_NB];
  Piece board[SQUARE_NB];

  int halfMoveClock;
  int gamePly;

  Key key;

  Bitboard blockersForKing[COLOR_NB];
  Bitboard pinners[COLOR_NB];

  /// <summary>
  /// What pieces of the opponent are attacking the king of the side to move
  /// </summary>
  Bitboard checkers;

  inline Bitboard pieces(PieceType pt) const {
    return byPieceBB[pt];
  }

  inline Bitboard pieces(Color c) const {
    return byColorBB[c];
  }

  inline Bitboard pieces(Color c, PieceType pt) const {
    return byColorBB[c] & byPieceBB[pt];
  }

  inline Bitboard pieces(Color c, PieceType pt0, PieceType pt1) const {
    return byColorBB[c] & (byPieceBB[pt0] | byPieceBB[pt1]);
  }

  inline Bitboard pieces(PieceType pt0, PieceType pt1) const {
    return byPieceBB[pt0] | byPieceBB[pt1];
  }

  inline Bitboard pieces() const {
    return byColorBB[WHITE] | byColorBB[BLACK];
  }

  inline Square kingSquare(Color c) const {
    return getLsb(pieces(c, KING));
  }

  inline CastlingRights castlingRightsOf(Color c) const {

    if (c == WHITE)
      return WHITE_CASTLING & castlingRights;

    return BLACK_CASTLING & castlingRights;
  }

  inline bool hasNonPawns(Color c) const {
    return pieces(c) & ~pieces(PAWN, KING);
  }

  Bitboard attackersTo(Square square, Bitboard occupied) const;
  Bitboard attackersTo(Square square, Color attackerColor, Bitboard occupied) const;
  Bitboard slidingAttackersTo(Square square, Color attackerColor, Bitboard occupied) const;

  inline Bitboard attackersTo(Square square, Color attackerColor) const {
    return attackersTo(square, attackerColor, pieces());
  }

  void updatePins(Color color);

  /// <summary>
  /// Invoke AFTER the side to move has been updated.
  /// Refreshes blockersForKing, pinners, checkers
  /// </summary>
  inline void updateAttacksToKings() {
    updatePins(WHITE);
    updatePins(BLACK);

    checkers = attackersTo(kingSquare(sideToMove), ~sideToMove);
  }

  void updateKey();

  /// <summary>
  /// Assmue there is a piece in the given square.
  /// Call this if you already know what piece was there
  /// </summary>
  inline void removePiece(Square sq, Piece pc, NNUE::Accumulator* acc) {
    key ^= RANDOM_ARRAY[64 * HASH_PIECE[pc] + sq];

    board[sq] = NO_PIECE;
    byColorBB[colorOf(pc)] ^= sq;
    byPieceBB[ptypeOf(pc)] ^= sq;

    acc->deactivateFeature(sq, pc);
  }

  /// <summary>
  /// Assmue there is not any piece in the given square
  /// </summary>
  inline void putPiece(Square sq, Piece pc, NNUE::Accumulator* acc) {
    key ^= RANDOM_ARRAY[64 * HASH_PIECE[pc] + sq];

    board[sq] = pc;
    byColorBB[colorOf(pc)] ^= sq;
    byPieceBB[ptypeOf(pc)] ^= sq;

    acc->activateFeature(sq, pc);
  }

  /// <summary>
  /// Assmue there is not any piece in the destination square
  /// Call this if you already know what piece was there
  /// </summary>
  inline void movePiece(Square from, Square to, Piece pc, NNUE::Accumulator* acc) {

    const int c0 = 64 * HASH_PIECE[pc];
    key ^= RANDOM_ARRAY[c0 + from] ^ RANDOM_ARRAY[c0 + to];

    board[from] = NO_PIECE;
    board[to] = pc;
    const Bitboard fromTo = from | to;
    byColorBB[colorOf(pc)] ^= fromTo;
    byPieceBB[ptypeOf(pc)] ^= fromTo;

    acc->moveFeature(from, to, pc);
  }

  bool isLegal(Move move);

  void doNullMove();

  void doMove(Move move, NNUE::Accumulator* accumulator);

  bool see_ge(Move m, Bitboard& occupied, Value threshold) const;

  bool see_ge(Move m, Value threshold) const;

  void setToFen(const string& fen, NNUE::Accumulator* accumulator);

  string toFenString() const;

  void updateAccumulator(NNUE::Accumulator* accumulator);
};

std::ostream& operator<<(std::ostream& stream, Position& sqr);