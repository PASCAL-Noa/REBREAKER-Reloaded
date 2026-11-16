#pragma once

#include "ECS/Entity.h"
#include "Data/Color.h"
#include "Math/Vector2.h"
#include <array>
#include <vector>
#include <utility>
#include <cstdint>

enum class TetrominoType : uint8_t
{
    None = 0,
    I = 1,
    J = 2,
    L = 3,
    O = 4,
    S = 5,
    T = 6,
    Z = 7,
    Count = 8
};

struct TetrisCell
{
    bool Occupied = false;
    TetrominoType Type = TetrominoType::None;
    Color CellColor{255, 255, 255, 255};
    Entity BrickEntity = NULL_ENTITY;
};

class TetrisMatrix
{
public:
    static constexpr int WIDTH = 10;
    static constexpr int HEIGHT = 20;
    static constexpr int TOP_DANGER_ROW = 3;
    static constexpr int BOTTOM_DANGER_ROW = 17;

    TetrisMatrix();

    void Reset();

    // 7-Bag Randomizer
    [[nodiscard]] TetrominoType GetNextPieceType() const noexcept { return m_nextType; }
    TetrominoType SpawnNextPiece();
    void SetNextPieceType(TetrominoType type) noexcept { m_nextType = type; }
    void SetRngSeed(uint32_t seed) noexcept;

    // Active piece control
    bool SpawnPiece(TetrominoType type);
    [[nodiscard]] bool HasActivePiece() const noexcept { return m_currentType != TetrominoType::None; }
    [[nodiscard]] TetrominoType GetCurrentType() const noexcept { return m_currentType; }
    [[nodiscard]] int GetCurrentX() const noexcept { return m_currentX; }
    [[nodiscard]] int GetCurrentY() const noexcept { return m_currentY; }
    [[nodiscard]] int GetCurrentRotation() const noexcept { return m_currentRot; }
    
    bool MoveLeft();
    bool MoveRight();
    bool StepDown();
    bool RotateClockwise();
    bool RotateCounterClockwise();
    std::vector<std::pair<int, int>> HardDrop();

    [[nodiscard]] int GetGhostY() const;
    [[nodiscard]] std::vector<std::pair<int, int>> GetActivePieceBlocks() const;
    [[nodiscard]] std::vector<std::pair<int, int>> GetGhostPieceBlocks() const;

    // Grid queries and mutations
    [[nodiscard]] bool CanPlace(TetrominoType type, int x, int y, int rot) const;
    [[nodiscard]] const TetrisCell& GetCell(int x, int y) const;
    void SetCell(int x, int y, const TetrisCell& cell);
    bool RemoveCell(int x, int y);

    // Settling & Line Clearing
    std::vector<std::pair<int, int>> SettlePiece();
    std::vector<int> CheckAndClearLines();

    // Danger & metrics
    [[nodiscard]] bool IsTopDangerActive() const;
    [[nodiscard]] bool IsDangerLineReached(int dangerRow = BOTTOM_DANGER_ROW) const;
    [[nodiscard]] int GetHighestOccupiedRow() const; // Lowest row index with blocks (0 is top)
    [[nodiscard]] int GetLowestOccupiedRow() const;  // Highest row index with blocks (19 is bottom)
    [[nodiscard]] int GetOccupiedCellCount() const;
    [[nodiscard]] int GetTotalLinesCleared() const noexcept { return m_totalLinesCleared; }

    // Static helpers for shapes & colors
    static std::array<std::pair<int, int>, 4> GetShapeOffsets(TetrominoType type, int rotation);
    static Color GetPieceColor(TetrominoType type);
    static const char* GetPieceName(TetrominoType type);

    // World transform conversions
    static Vector2f GridToWorld(int col, int row, const Vector2f& matrixOrigin, const Vector2f& cellSize);
    static std::pair<int, int> WorldToGrid(const Vector2f& worldPos, const Vector2f& matrixOrigin, const Vector2f& cellSize);

private:
    void RefillBag();
    TetrominoType PopFromBag();

    std::array<std::array<TetrisCell, WIDTH>, HEIGHT> m_grid{};
    TetrominoType m_currentType = TetrominoType::None;
    int m_currentX = 3;
    int m_currentY = 0;
    int m_currentRot = 0;

    TetrominoType m_nextType = TetrominoType::None;
    std::vector<TetrominoType> m_bag;
    uint32_t m_rngSeed = 1337420u;
    int m_totalLinesCleared = 0;
};
