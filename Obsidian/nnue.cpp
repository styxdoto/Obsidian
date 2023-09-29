#include "nnue.h"
#include "bitboard.h"

#include <iostream>
#include <fstream>

using namespace std;

namespace NNUE {

#ifdef USE_AVX2

  using Vec = __m256i;
  constexpr int WeightsPerVec = sizeof(Vec) / sizeof(int16_t);

#else

#endif

  int FeatureIndexTable[COLOR_NB][PIECE_NB][SQUARE_NB];

  struct {
	int16_t FeatureWeights[FeatureDimensions * TransformedFeatureDimensions];
	int16_t FeatureBiases[TransformedFeatureDimensions];
	int16_t OutputWeights[2 * TransformedFeatureDimensions];
	int16_t OutputBias;
  } Content;

#ifdef USE_AVX2

  inline int vecHadd(Vec vec) {
	__m128i xmm0;
	__m128i xmm1;

	// Get the lower and upper half of the register:
	xmm0 = _mm256_castsi256_si128(vec);
	xmm1 = _mm256_extracti128_si256(vec, 1);

	// Add the lower and upper half vertically:
	xmm0 = _mm_add_epi32(xmm0, xmm1);

	// Get the upper half of the result:
	xmm1 = _mm_unpackhi_epi64(xmm0, xmm0);

	// Add the lower and upper half vertically:
	xmm0 = _mm_add_epi32(xmm0, xmm1);

	// Shuffle the result so that the lower 32-bits are directly above the second-lower 32-bits:
	xmm1 = _mm_shuffle_epi32(xmm0, _MM_SHUFFLE(2, 3, 0, 1));

	// Add the lower 32-bits to the second-lower 32-bits vertically:
	xmm0 = _mm_add_epi32(xmm0, xmm1);

	// Cast the result to the 32-bit integer type and return it:
	return _mm_cvtsi128_si32(xmm0);
  }

  template <int InputSize>
  inline void addToAll(int16_t* input, int offset)
  {
	offset /= WeightsPerVec;

	Vec* inputVec = (Vec*)input;
	Vec* weightsVec = (Vec*)Content.FeatureWeights;

	for (int i = 0; i < InputSize / WeightsPerVec; ++i) {
	  inputVec[i] = _mm256_add_epi16(inputVec[i], weightsVec[offset + i]);
	}
  }

  template <int InputSize>
  inline void subtractFromAll(int16_t* input, int offset)
  {
	offset /= WeightsPerVec;

	Vec* inputVec = (Vec*)input;
	Vec* weightsVec = (Vec*)Content.FeatureWeights;

	for (int i = 0; i < InputSize / WeightsPerVec; ++i) {
	  inputVec[i] = _mm256_sub_epi16(inputVec[i], weightsVec[offset + i]);
	}
  }

  template <int InputSize>
  inline void addAndSubtractFromAll(int16_t* input, int addOff, int subtractOff) {
	addOff /= WeightsPerVec;
	subtractOff /= WeightsPerVec;

	Vec* inputVec = (Vec*)input;
	Vec* weightsVec = (Vec*)Content.FeatureWeights;

	for (int i = 0; i < InputSize / WeightsPerVec; ++i) {
	  inputVec[i] = _mm256_sub_epi16(_mm256_add_epi16(inputVec[i], weightsVec[addOff + i]), weightsVec[subtractOff + i]);
	}
  }
#else
  template <int InputSize>
  inline void addToAll(int16_t* input, int offset)
  {
	for (int i = 0; i < InputSize; ++i)
	  input[i] += Content.FeatureWeights[offset + i];
  }

  template <int InputSize>
  inline void subtractFromAll(int16_t* input, int offset)
  {
	for (int i = 0; i < InputSize; ++i)
	  input[i] -= Content.FeatureWeights[offset + i];
  }

  template <int InputSize>
  inline void addAndSubtractFromAll(int16_t* input, int addOff, int subtractOff) {
	for (int i = 0; i < InputSize; ++i)
	  input[i] += Content.FeatureWeights[addOff + i] - Content.FeatureWeights[subtractOff + i];
  }
#endif // USE_AVX2

  void Accumulator::reset() {
	memcpy(white, Content.FeatureBiases, sizeof(Content.FeatureBiases));
	memcpy(black, Content.FeatureBiases, sizeof(Content.FeatureBiases));
  }

  void Accumulator::activateFeature(Square sq, Piece pc) {
	addToAll<TransformedFeatureDimensions>(white,
	  TransformedFeatureDimensions * FeatureIndexTable[WHITE][pc][sq]);

	addToAll<TransformedFeatureDimensions>(black,
	  TransformedFeatureDimensions * FeatureIndexTable[BLACK][pc][sq]);
  }

  void Accumulator::deactivateFeature(Square sq, Piece pc) {
	subtractFromAll<TransformedFeatureDimensions>(white,
	  TransformedFeatureDimensions * FeatureIndexTable[WHITE][pc][sq]);

	subtractFromAll<TransformedFeatureDimensions>(black,
	  TransformedFeatureDimensions * FeatureIndexTable[BLACK][pc][sq]);
  }

  void Accumulator::moveFeature(Square from, Square to, Piece pc) {
	addAndSubtractFromAll<TransformedFeatureDimensions>(white,
	  TransformedFeatureDimensions * FeatureIndexTable[WHITE][pc][to],
	  TransformedFeatureDimensions * FeatureIndexTable[WHITE][pc][from]);

	addAndSubtractFromAll<TransformedFeatureDimensions>(black,
	  TransformedFeatureDimensions * FeatureIndexTable[BLACK][pc][to],
	  TransformedFeatureDimensions * FeatureIndexTable[BLACK][pc][from]);
  }

  void load(const char* file) {
	ifstream stream(file, ios::binary);

	if (!bool(stream)) {
	  cout << "Failed to load NNUE" << endl;
	  exit(1);
	}

	stream.read((char*)&Content, sizeof(Content));

	// Cache feature indexes
	for (int pt = PAWN; pt <= KING; ++pt) {
	  for (Square sq = SQ_A1; sq < SQUARE_NB; ++sq) {
		Piece whitePc = make_piece(WHITE, PieceType(pt));
		Piece blackPc = make_piece(BLACK, PieceType(pt));

		FeatureIndexTable[WHITE][whitePc][sq] = SQUARE_NB * (pt - 1) + sq;
		FeatureIndexTable[WHITE][blackPc][sq] = SQUARE_NB * (pt + 5) + sq;

		FeatureIndexTable[BLACK][whitePc][sq] = SQUARE_NB * (pt + 5) + flip_rank(sq);
		FeatureIndexTable[BLACK][blackPc][sq] = SQUARE_NB * (pt - 1) + flip_rank(sq);
	  }
	}
  }

  inline int clippedRelu(int16_t x) {
	if (x < 0)
	  return 0;
	if (x > 255)
	  return 255;
	return x;
  }

  Value evaluate(Accumulator* accumulator, Color sideToMove) {
	int16_t* stmAccumulator;
	int16_t* oppAccumulator;

	if (sideToMove == WHITE) {
	  stmAccumulator = accumulator->white;
	  oppAccumulator = accumulator->black;
	}
	else {
	  stmAccumulator = accumulator->black;
	  oppAccumulator = accumulator->white;
	}

	int sum = Content.OutputBias;

#ifdef USE_AVX2

	const Vec reluClipMin = _mm256_setzero_si256();
	const Vec reluClipMax = _mm256_set1_epi16(255);

	Vec* stmAccVec = (Vec*)stmAccumulator;
	Vec* oppAccVec = (Vec*)oppAccumulator;
	Vec* stmWeightsVec = (Vec*)Content.OutputWeights;
	Vec* oppWeightsVec = (Vec*)&Content.OutputWeights[TransformedFeatureDimensions];

	Vec sumVec = _mm256_setzero_si256();

	for (int i = 0; i < TransformedFeatureDimensions / WeightsPerVec; ++i) {

	  { // Side to move
		Vec crelu = _mm256_min_epi16(_mm256_max_epi16(stmAccVec[i], reluClipMin), reluClipMax);
		Vec stmProduct = _mm256_madd_epi16(crelu, stmWeightsVec[i]);
		sumVec = _mm256_add_epi32(sumVec, stmProduct);
	  }
	  { // Non side to move
		Vec crelu = _mm256_min_epi16(_mm256_max_epi16(oppAccVec[i], reluClipMin), reluClipMax);
		Vec oppProduct = _mm256_madd_epi16(crelu, oppWeightsVec[i]);
		sumVec = _mm256_add_epi32(sumVec, oppProduct);
	  }
	}

	sum += vecHadd(sumVec);

#else

	for (int i = 0; i < TransformedFeatureDimensions; ++i) {
	  sum += clippedRelu(stmAccumulator[i]) * Content.OutputWeights[i];
	  sum += clippedRelu(oppAccumulator[i]) * Content.OutputWeights[TransformedFeatureDimensions + i];
	}

#endif // USE_AVX2

	return Value(sum / 40);
  }

}
