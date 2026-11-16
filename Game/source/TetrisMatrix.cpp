#include "Tetris/TetrisMatrix.h"
#include <algorithm>
#include <cmath>

TetrisMatrix::TetrisMatrix()
{
    Reset();
}

void TetrisMatrix::Reset()
{
    for (auto& row : m_grid)
    {
        row.fill(TetrisCell{});
    }

    m_currentType = TetrominoType::None;
    m_currentX = 3;
    m_currentY = 0;
    m_currentRot = 0;
    m_totalLinesCleared = 0;

    m_bag.clear();
    RefillBag();
    m_nextType = PopFromBag();
}

void TetrisMatrix::SetRngSeed(uint32_t seed) noexcept
{
    m_rngSeed = (seed == 0) ? 1337420u : seed;
    m_bag.clear();
    RefillBag();
    m_nextType = PopFromBag();
}

void TetrisMatrix::RefillBag()
{
    m_bag = {
        TetrominoType::I,
        TetrominoType::J,
        TetrominoType::L,
        TetrominoType::O,
        TetrominoType::S,
        TetrominoType::T,
        TetrominoType::Z
    };

    // Deterministic shuffle using Linear Congruential PRNG
    for (size_t i = m_bag.size() - 1; i > 0; --i)
    {
        m_rngSeed = m_rngSeed * 1664525u + 1013904223u;
        size_t j = static_cast<size_t>((m_rngSeed >> 16) % (i + 1));
        std::swap(m_bag[i], m_bag[j]);
    }
}

TetrominoType TetrisMatrix::PopFromBag()
{
    if (m_bag.empty())
    {
        RefillBag();
    }
    TetrominoType piece = m_bag.back();
    m_bag.pop_back();
    return piece;
}

TetrominoType TetrisMatrix::SpawnNextPiece()
{
    if (m_nextType == TetrominoType::None)
    {
        m_nextType = PopFromBag();
    }

    TetrominoType next = m_nextType;
    m_nextType = PopFromBag();
    SpawnPiece(next);
    return next;
}

bool TetrisMatrix::SpawnPiece(TetrominoType type)
{
    m_currentType = type;
    m_currentRot = 0;
    m_currentX = (WIDTH - 4) / 2; // Col 3 for 10-wide matrix
    m_currentY = 0;

    return CanPlace(m_currentType, m_currentX, m_currentY, m_currentRot);
}

bool TetrisMatrix::CanPlace(TetrominoType type, int posX, int posY, int rot) const
{
    if (type == TetrominoType::None)
    {
        return false;
    }

    const auto offsets = GetShapeOffsets(type, rot);
    for (const auto& [dx, dy] : offsets)
    {
        int x = posX + dx;
        int y = posY + dy;

        if (x < 0 || x >= WIDTH || y >= HEIGHT)
        {
            return false;
        }

        if (y >= 0 && m_grid[y][x].Occupied)
        {
            return false;
        }
    }

    return true;
}

bool TetrisMatrix::MoveLeft()
{
    if (m_currentType == TetrominoType::None) return false;
    if (CanPlace(m_currentType, m_currentX - 1, m_currentY, m_currentRot))
    {
        --m_currentX;
        return true;
    }
    return false;
}

bool TetrisMatrix::MoveRight()
{
    if (m_currentType == TetrominoType::None) return false;
    if (CanPlace(m_currentType, m_currentX + 1, m_currentY, m_currentRot))
    {
        ++m_currentX;
        return true;
    }
    return false;
}

bool TetrisMatrix::MoveTowardsColumn(int targetCol)
{
    if (m_currentType == TetrominoType::None) return false;
    if (m_currentX < targetCol)
    {
        return MoveRight();
    }
    else if (m_currentX > targetCol)
    {
        return MoveLeft();
    }
    return false;
}

bool TetrisMatrix::StepDown()
{
    if (m_currentType == TetrominoType::None) return false;
    if (CanPlace(m_currentType, m_currentX, m_currentY + 1, m_currentRot))
    {
        ++m_currentY;
        return true;
    }
    return false;
}

bool TetrisMatrix::RotateClockwise()
{
    if (m_currentType == TetrominoType::None) return false;
    int nextRot = (m_currentRot + 1) % 4;

    const std::pair<int, int> kicks[] = {
        {0, 0}, {-1, 0}, {1, 0}, {-2, 0}, {2, 0}, {0, -1}
    };

    for (const auto& [kx, ky] : kicks)
    {
        if (CanPlace(m_currentType, m_currentX + kx, m_currentY + ky, nextRot))
        {
            m_currentX += kx;
            m_currentY += ky;
            m_currentRot = nextRot;
            return true;
        }
    }

    return false;
}

bool TetrisMatrix::RotateCounterClockwise()
{
    if (m_currentType == TetrominoType::None) return false;
    int nextRot = (m_currentRot + 3) % 4;

    const std::pair<int, int> kicks[] = {
        {0, 0}, {1, 0}, {-1, 0}, {2, 0}, {-2, 0}, {0, -1}
    };

    for (const auto& [kx, ky] : kicks)
    {
        if (CanPlace(m_currentType, m_currentX + kx, m_currentY + ky, nextRot))
        {
            m_currentX += kx;
            m_currentY += ky;
            m_currentRot = nextRot;
            return true;
        }
    }

    return false;
}

int TetrisMatrix::GetGhostY() const
{
    if (m_currentType == TetrominoType::None) return m_currentY;

    int ghostY = m_currentY;
    while (CanPlace(m_currentType, m_currentX, ghostY + 1, m_currentRot))
    {
        ++ghostY;
    }

    return ghostY;
}

std::vector<std::pair<int, int>> TetrisMatrix::HardDrop()
{
    if (m_currentType == TetrominoType::None) return {};
    m_currentY = GetGhostY();
    return SettlePiece();
}

std::vector<std::pair<int, int>> TetrisMatrix::GetActivePieceBlocks() const
{
    std::vector<std::pair<int, int>> blocks;
    if (m_currentType == TetrominoType::None) return blocks;

    const auto offsets = GetShapeOffsets(m_currentType, m_currentRot);
    for (const auto& [dx, dy] : offsets)
    {
        blocks.push_back({m_currentX + dx, m_currentY + dy});
    }

    return blocks;
}

std::vector<std::pair<int, int>> TetrisMatrix::GetGhostPieceBlocks() const
{
    std::vector<std::pair<int, int>> blocks;
    if (m_currentType == TetrominoType::None) return blocks;

    int ghostY = GetGhostY();
    const auto offsets = GetShapeOffsets(m_currentType, m_currentRot);
    for (const auto& [dx, dy] : offsets)
    {
        blocks.push_back({m_currentX + dx, ghostY + dy});
    }

    return blocks;
}

const TetrisCell& TetrisMatrix::GetCell(int x, int y) const
{
    static const TetrisCell s_emptyCell{};
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT)
    {
        return s_emptyCell;
    }
    return m_grid[y][x];
}

void TetrisMatrix::SetCell(int x, int y, const TetrisCell& cell)
{
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
    {
        m_grid[y][x] = cell;
    }
}

bool TetrisMatrix::RemoveCell(int x, int y)
{
    if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT && m_grid[y][x].Occupied)
    {
        m_grid[y][x] = TetrisCell{};
        return true;
    }
    return false;
}

std::vector<std::pair<int, int>> TetrisMatrix::SettlePiece()
{
    std::vector<std::pair<int, int>> settled;
    if (m_currentType == TetrominoType::None) return settled;

    const auto offsets = GetShapeOffsets(m_currentType, m_currentRot);
    Color color = GetPieceColor(m_currentType);

    for (const auto& [dx, dy] : offsets)
    {
        int x = m_currentX + dx;
        int y = m_currentY + dy;
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT)
        {
            m_grid[y][x].Occupied = true;
            m_grid[y][x].Type = m_currentType;
            m_grid[y][x].CellColor = color;
            m_grid[y][x].BrickEntity = NULL_ENTITY;
            settled.push_back({x, y});
        }
    }

    m_currentType = TetrominoType::None;
    return settled;
}

std::vector<int> TetrisMatrix::CheckAndClearLines()
{
    std::vector<int> clearedRows;
    for (int y = HEIGHT - 1; y >= 0; --y)
    {
        bool full = true;
        for (int x = 0; x < WIDTH; ++x)
        {
            if (!m_grid[y][x].Occupied)
            {
                full = false;
                break;
            }
        }
        if (full)
        {
            clearedRows.push_back(y);
        }
    }

    if (!clearedRows.empty())
    {
        m_totalLinesCleared += static_cast<int>(clearedRows.size());
        int destY = HEIGHT - 1;

        for (int srcY = HEIGHT - 1; srcY >= 0; --srcY)
        {
            bool wasCleared = (std::find(clearedRows.begin(), clearedRows.end(), srcY) != clearedRows.end());
            if (!wasCleared)
            {
                if (destY != srcY)
                {
                    for (int x = 0; x < WIDTH; ++x)
                    {
                        m_grid[destY][x] = m_grid[srcY][x];
                    }
                }
                --destY;
            }
        }

        for (int y = destY; y >= 0; --y)
        {
            for (int x = 0; x < WIDTH; ++x)
            {
                m_grid[y][x] = TetrisCell{};
            }
        }
    }

    return clearedRows;
}

bool TetrisMatrix::IsTopDangerActive() const
{
    return GetHighestOccupiedRow() <= TOP_DANGER_ROW;
}

bool TetrisMatrix::IsDangerLineReached(int dangerRow) const
{
    for (int y = dangerRow; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            if (m_grid[y][x].Occupied)
            {
                return true;
            }
        }
    }
    return false;
}

int TetrisMatrix::GetHighestOccupiedRow() const
{
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            if (m_grid[y][x].Occupied)
            {
                return y;
            }
        }
    }
    return HEIGHT; // Completely empty
}

int TetrisMatrix::GetLowestOccupiedRow() const
{
    for (int y = HEIGHT - 1; y >= 0; --y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            if (m_grid[y][x].Occupied)
            {
                return y;
            }
        }
    }
    return -1; // Completely empty
}

int TetrisMatrix::GetOccupiedCellCount() const
{
    int count = 0;
    for (int y = 0; y < HEIGHT; ++y)
    {
        for (int x = 0; x < WIDTH; ++x)
        {
            if (m_grid[y][x].Occupied)
            {
                ++count;
            }
        }
    }
    return count;
}

std::array<std::pair<int, int>, 4> TetrisMatrix::GetShapeOffsets(TetrominoType type, int rotation)
{
    int r = (rotation % 4 + 4) % 4;

    switch (type)
    {
    case TetrominoType::I:
        switch (r)
        {
        case 0: return {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}};
        case 1: return {{{2, 0}, {2, 1}, {2, 2}, {2, 3}}};
        case 2: return {{{0, 2}, {1, 2}, {2, 2}, {3, 2}}};
        case 3: default: return {{{1, 0}, {1, 1}, {1, 2}, {1, 3}}};
        }

    case TetrominoType::J:
        switch (r)
        {
        case 0: return {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}};
        case 1: return {{{1, 0}, {2, 0}, {1, 1}, {1, 2}}};
        case 2: return {{{0, 1}, {1, 1}, {2, 1}, {2, 2}}};
        case 3: default: return {{{1, 0}, {1, 1}, {0, 2}, {1, 2}}};
        }

    case TetrominoType::L:
        switch (r)
        {
        case 0: return {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}};
        case 1: return {{{1, 0}, {1, 1}, {1, 2}, {2, 2}}};
        case 2: return {{{0, 1}, {1, 1}, {2, 1}, {0, 2}}};
        case 3: default: return {{{0, 0}, {1, 0}, {1, 1}, {1, 2}}};
        }

    case TetrominoType::O:
        return {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}};

    case TetrominoType::S:
        switch (r)
        {
        case 0: return {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}};
        case 1: return {{{1, 0}, {1, 1}, {2, 1}, {2, 2}}};
        case 2: return {{{1, 1}, {2, 1}, {0, 2}, {1, 2}}};
        case 3: default: return {{{0, 0}, {0, 1}, {1, 1}, {1, 2}}};
        }

    case TetrominoType::T:
        switch (r)
        {
        case 0: return {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}};
        case 1: return {{{1, 0}, {1, 1}, {2, 1}, {1, 2}}};
        case 2: return {{{0, 1}, {1, 1}, {2, 1}, {1, 2}}};
        case 3: default: return {{{1, 0}, {0, 1}, {1, 1}, {1, 2}}};
        }

    case TetrominoType::Z:
        switch (r)
        {
        case 0: return {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}};
        case 1: return {{{2, 0}, {1, 1}, {2, 1}, {1, 2}}};
        case 2: return {{{0, 1}, {1, 1}, {1, 2}, {2, 2}}};
        case 3: default: return {{{1, 0}, {0, 1}, {1, 1}, {0, 2}}};
        }

    default:
        return {{{0, 0}, {0, 0}, {0, 0}, {0, 0}}};
    }
}

Color TetrisMatrix::GetPieceColor(TetrominoType type)
{
    switch (type)
    {
    case TetrominoType::I: return Color{0, 240, 255, 255};    // Electric Cyan
    case TetrominoType::J: return Color{35, 100, 255, 255};   // Deep Azure Blue
    case TetrominoType::L: return Color{255, 140, 20, 255};   // Neon Orange
    case TetrominoType::O: return Color{255, 225, 30, 255};   // Solar Yellow
    case TetrominoType::S: return Color{40, 235, 90, 255};    // Acid Green
    case TetrominoType::T: return Color{195, 45, 255, 255};   // Plasma Purple
    case TetrominoType::Z: return Color{255, 45, 65, 255};    // Laser Red
    default: return Colors::White;
    }
}

const char* TetrisMatrix::GetPieceName(TetrominoType type)
{
    switch (type)
    {
    case TetrominoType::I: return "I";
    case TetrominoType::J: return "J";
    case TetrominoType::L: return "L";
    case TetrominoType::O: return "O";
    case TetrominoType::S: return "S";
    case TetrominoType::T: return "T";
    case TetrominoType::Z: return "Z";
    default: return "None";
    }
}

Vector2f TetrisMatrix::GridToWorld(int col, int row, const Vector2f& matrixOrigin, const Vector2f& cellSize)
{
    const float totalW = static_cast<float>(WIDTH) * cellSize.X;
    const float totalH = static_cast<float>(HEIGHT) * cellSize.Y;
    const float startX = matrixOrigin.X - (totalW * 0.5f) + (cellSize.X * 0.5f);
    const float startY = matrixOrigin.Y - (totalH * 0.5f) + (cellSize.Y * 0.5f);

    return Vector2f{
        startX + static_cast<float>(col) * cellSize.X,
        startY + static_cast<float>(row) * cellSize.Y
    };
}

std::pair<int, int> TetrisMatrix::WorldToGrid(const Vector2f& worldPos, const Vector2f& matrixOrigin, const Vector2f& cellSize)
{
    const float totalW = static_cast<float>(WIDTH) * cellSize.X;
    const float totalH = static_cast<float>(HEIGHT) * cellSize.Y;
    const float startX = matrixOrigin.X - (totalW * 0.5f) + (cellSize.X * 0.5f);
    const float startY = matrixOrigin.Y - (totalH * 0.5f) + (cellSize.Y * 0.5f);

    int col = static_cast<int>(std::round((worldPos.X - startX) / cellSize.X));
    int row = static_cast<int>(std::round((worldPos.Y - startY) / cellSize.Y));

    return {col, row};
}

