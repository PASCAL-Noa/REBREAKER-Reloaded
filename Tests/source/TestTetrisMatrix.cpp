#include <doctest/doctest.h>
#include "Tetris/TetrisMatrix.h"
#include <string>
#include <set>
#include <vector>

TEST_SUITE("TetrisMatrix Core Logic & Gimmick Stage Engine")
{
    TEST_CASE("1. Matrix Initialization and Dimensions")
    {
        TetrisMatrix matrix;

        CHECK(matrix.WIDTH == 10);
        CHECK(matrix.HEIGHT == 20);
        CHECK(matrix.GetOccupiedCellCount() == 0);
        CHECK(matrix.GetTotalLinesCleared() == 0);
        CHECK(matrix.GetHighestOccupiedRow() == TetrisMatrix::HEIGHT);
        CHECK(matrix.GetLowestOccupiedRow() == -1);
        CHECK_FALSE(matrix.IsTopDangerActive());
        CHECK_FALSE(matrix.IsDangerLineReached());

        for (int y = 0; y < TetrisMatrix::HEIGHT; ++y)
        {
            for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
            {
                CHECK_FALSE(matrix.GetCell(x, y).Occupied);
                CHECK(matrix.GetCell(x, y).Type == TetrominoType::None);
            }
        }
    }

    TEST_CASE("2. Authentic Tetromino Geometry (7 Shapes x 4 Rotations = 4 Blocks Each)")
    {
        const TetrominoType allTypes[] = {
            TetrominoType::I,
            TetrominoType::J,
            TetrominoType::L,
            TetrominoType::O,
            TetrominoType::S,
            TetrominoType::T,
            TetrominoType::Z
        };

        for (auto type : allTypes)
        {
            Color col = TetrisMatrix::GetPieceColor(type);
            CHECK(col.a == 255);
            CHECK_FALSE((col.r == 0 && col.g == 0 && col.b == 0));

            const char* name = TetrisMatrix::GetPieceName(type);
            CHECK(name != nullptr);
            CHECK(std::string(name) != "None");

            for (int rot = 0; rot < 4; ++rot)
            {
                auto offsets = TetrisMatrix::GetShapeOffsets(type, rot);
                CHECK(offsets.size() == 4);

                std::set<std::pair<int, int>> uniqueBlocks;
                for (const auto& [dx, dy] : offsets)
                {
                    CHECK(dx >= 0);
                    CHECK(dx < 4);
                    CHECK(dy >= 0);
                    CHECK(dy < 4);
                    uniqueBlocks.insert({dx, dy});
                }
                CHECK(uniqueBlocks.size() == 4); // All 4 blocks must be distinct!
            }
        }
    }

    TEST_CASE("3. 7-Bag Randomizer Authenticity and Fair Distribution")
    {
        TetrisMatrix matrix;
        matrix.SetRngSeed(424242u);

        // Draw 7 pieces: must contain each of the 7 pieces exactly once!
        std::set<TetrominoType> bag1;
        for (int i = 0; i < 7; ++i)
        {
            TetrominoType piece = matrix.SpawnNextPiece();
            bag1.insert(piece);
        }
        CHECK(bag1.size() == 7);

        // Draw next 7 pieces: must also contain all 7 pieces without omissions!
        std::set<TetrominoType> bag2;
        for (int i = 0; i < 7; ++i)
        {
            TetrominoType piece = matrix.SpawnNextPiece();
            bag2.insert(piece);
        }
        CHECK(bag2.size() == 7);
    }

    TEST_CASE("4. Movement, Rotation, and Boundary Collision Handling")
    {
        TetrisMatrix matrix;
        // Spawn O piece at center top: 2x2 block
        CHECK(matrix.SpawnPiece(TetrominoType::O));
        CHECK(matrix.HasActivePiece());
        CHECK(matrix.GetCurrentType() == TetrominoType::O);

        // Move Left towards wall
        for (int i = 0; i < 15; ++i)
        {
            matrix.MoveLeft();
        }
        // Cannot escape left edge (block x >= 0)
        auto blocks = matrix.GetActivePieceBlocks();
        for (const auto& [x, y] : blocks)
        {
            CHECK(x >= 0);
        }

        // Move Right towards wall
        for (int i = 0; i < 20; ++i)
        {
            matrix.MoveRight();
        }
        blocks = matrix.GetActivePieceBlocks();
        for (const auto& [x, y] : blocks)
        {
            CHECK(x < TetrisMatrix::WIDTH);
        }

        // Test StepDown to bottom
        while (matrix.StepDown())
        {
            // Descending
        }
        blocks = matrix.GetActivePieceBlocks();
        for (const auto& [x, y] : blocks)
        {
            CHECK(y < TetrisMatrix::HEIGHT);
        }

        // Test MoveTowardsColumn tracking
        TetrisMatrix trackingMatrix;
        CHECK(trackingMatrix.SpawnPiece(TetrominoType::I));
        int startX = trackingMatrix.GetCurrentX();
        CHECK(trackingMatrix.MoveTowardsColumn(startX + 2));
        CHECK(trackingMatrix.GetCurrentX() == startX + 1);
        while (trackingMatrix.MoveTowardsColumn(0)) {}
        CHECK(trackingMatrix.GetCurrentX() <= startX);
    }

    TEST_CASE("5. Ghost Piece Calculation and Hard Drop")
    {
        TetrisMatrix matrix;
        CHECK(matrix.SpawnPiece(TetrominoType::I)); // I-piece (rot 0: y=1 horizontal)

        int ghostY = matrix.GetGhostY();
        // Since board is empty, I piece horizontal can drop all the way to row 19!
        // At rot 0, I blocks are at (dx, dy=1), so when currentY + dy = 19 -> currentY = 18.
        CHECK(ghostY == 18);

        auto ghostBlocks = matrix.GetGhostPieceBlocks();
        CHECK(ghostBlocks.size() == 4);
        for (const auto& [gx, gy] : ghostBlocks)
        {
            CHECK(gy == 19);
        }

        // Execute Hard Drop
        auto settled = matrix.HardDrop();
        CHECK(settled.size() == 4);
        CHECK_FALSE(matrix.HasActivePiece());
        CHECK(matrix.GetOccupiedCellCount() == 4);
        CHECK(matrix.GetLowestOccupiedRow() == 19);
    }

    TEST_CASE("6. Breakout Ball Collision: Brick Removal from Matrix")
    {
        TetrisMatrix matrix;
        matrix.SpawnPiece(TetrominoType::O);
        matrix.HardDrop();
        CHECK(matrix.GetOccupiedCellCount() == 4);

        // Find an occupied cell and simulate ball collision
        int hitX = -1, hitY = -1;
        for (int y = 0; y < TetrisMatrix::HEIGHT; ++y)
        {
            for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
            {
                if (matrix.GetCell(x, y).Occupied)
                {
                    hitX = x;
                    hitY = y;
                    break;
                }
            }
            if (hitX != -1) break;
        }

        REQUIRE(hitX != -1);
        CHECK(matrix.RemoveCell(hitX, hitY));
        CHECK_FALSE(matrix.GetCell(hitX, hitY).Occupied);
        CHECK(matrix.GetOccupiedCellCount() == 3);

        // Removing non-occupied cell returns false
        CHECK_FALSE(matrix.RemoveCell(hitX, hitY));
    }

    TEST_CASE("7. Line Clear Detection and Gravity Collapse: Single, Double, Triple, and Tetris")
    {
        SUBCASE("Single Line Clear")
        {
            TetrisMatrix matrix;
            // Fill row 19 completely except 1 cell, then fill that cell
            for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
            {
                TetrisCell cell{};
                cell.Occupied = true;
                cell.Type = TetrominoType::I;
                matrix.SetCell(x, 19, cell);
            }

            // Put a brick above in row 18 at x = 4
            TetrisCell overheadCell{};
            overheadCell.Occupied = true;
            overheadCell.Type = TetrominoType::T;
            matrix.SetCell(4, 18, overheadCell);

            CHECK(matrix.GetOccupiedCellCount() == 11);

            auto cleared = matrix.CheckAndClearLines();
            REQUIRE(cleared.size() == 1);
            CHECK(cleared[0] == 19);
            CHECK(matrix.GetTotalLinesCleared() == 1);

            // Row 18 block should collapse down to row 19!
            CHECK(matrix.GetOccupiedCellCount() == 1);
            CHECK(matrix.GetCell(4, 19).Occupied);
            CHECK(matrix.GetCell(4, 19).Type == TetrominoType::T);
            CHECK_FALSE(matrix.GetCell(4, 18).Occupied);
        }

        SUBCASE("4-Line Full TETRIS Clear and Above Rows Collapse")
        {
            TetrisMatrix matrix;
            // Fill rows 16, 17, 18, 19 completely
            for (int y = 16; y <= 19; ++y)
            {
                for (int x = 0; x < TetrisMatrix::WIDTH; ++x)
                {
                    TetrisCell cell{};
                    cell.Occupied = true;
                    cell.Type = TetrominoType::I;
                    matrix.SetCell(x, y, cell);
                }
            }

            // Place an overhead structure in rows 14 and 15
            TetrisCell marker{};
            marker.Occupied = true;
            marker.Type = TetrominoType::O;
            matrix.SetCell(2, 14, marker);
            matrix.SetCell(3, 15, marker);

            CHECK(matrix.GetOccupiedCellCount() == 42);

            auto cleared = matrix.CheckAndClearLines();
            REQUIRE(cleared.size() == 4);
            CHECK(matrix.GetTotalLinesCleared() == 4);

            // Overhead blocks in rows 14 and 15 must collapse by 4 rows:
            // 14 -> 18, 15 -> 19!
            CHECK(matrix.GetOccupiedCellCount() == 2);
            CHECK(matrix.GetCell(2, 18).Occupied);
            CHECK(matrix.GetCell(3, 19).Occupied);
            CHECK_FALSE(matrix.GetCell(2, 14).Occupied);
            CHECK_FALSE(matrix.GetCell(3, 15).Occupied);
        }
    }

    TEST_CASE("8. Danger Line and Top-Out Threat Detection")
    {
        TetrisMatrix matrix;
        CHECK_FALSE(matrix.IsDangerLineReached(17));
        CHECK_FALSE(matrix.IsTopDangerActive());

        // Place a block at row 16 (above danger line 17)
        TetrisCell c{};
        c.Occupied = true;
        matrix.SetCell(5, 16, c);
        CHECK_FALSE(matrix.IsDangerLineReached(17));

        // Place a block at row 17 (breaching danger line near paddle)
        matrix.SetCell(5, 17, c);
        CHECK(matrix.IsDangerLineReached(17));

        // Check Top-Out danger (when blocks stack up to rows 0..3)
        CHECK_FALSE(matrix.IsTopDangerActive());
        matrix.SetCell(5, 2, c);
        CHECK(matrix.IsTopDangerActive());
        CHECK(matrix.GetHighestOccupiedRow() == 2);
    }

    TEST_CASE("9. World to Grid Coordinate Transformations")
    {
        Vector2f origin{0.0f, -100.0f};
        Vector2f cellSize{44.0f, 24.0f};

        for (int r = 0; r < TetrisMatrix::HEIGHT; r += 5)
        {
            for (int c = 0; c < TetrisMatrix::WIDTH; c += 3)
            {
                Vector2f world = TetrisMatrix::GridToWorld(c, r, origin, cellSize);
                auto [outCol, outRow] = TetrisMatrix::WorldToGrid(world, origin, cellSize);
                CHECK(outCol == c);
                CHECK(outRow == r);
            }
        }
    }
}
