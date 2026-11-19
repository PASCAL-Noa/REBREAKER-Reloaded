#pragma once
#include "Data/Color.h"
#include <string>
#include <vector>

struct DialogueCard
{
    std::string SpeakerName;
    std::string SpeakerRole;
    Color       SpeakerColor{80, 220, 255, 255};
    std::string Text;
};

struct StoryChapter
{
    int         ChapterIndex = 0;       // 0-indexed (Chapter 1 = 0, Chapter 2 = 1, Chapter 3 = 2)
    std::string ChapterNumberStr = "CHAPTER 01";
    std::string Title = "SECTOR ZERO: SYSTEM BREACH";
    std::string Subtitle = "PADDLE ALLOY CALIBRATION";
    std::string Objective = "Survive hostiles, test paddle durability, and clear the defensive matrix!";
    int         CurrencyReward = 500;
    std::string UnlockedLoreId = "lore_ch1";
    std::string UnlockedCosmeticId = "";
    bool        IsBossChapter = false;

    std::vector<DialogueCard> IntroDialogue;
    std::vector<DialogueCard> OutroDialogue;
};

class StoryData
{
public:
    static std::vector<StoryChapter> GetDefaultChapters()
    {
        std::vector<StoryChapter> chapters;

        // Chapter 1: Sector Zero
        {
            StoryChapter ch;
            ch.ChapterIndex = 0;
            ch.ChapterNumberStr = "CHAPTER 01";
            ch.Title = "SECTOR ZERO: SYSTEM BREACH";
            ch.Subtitle = "ALLOY DURABILITY CALIBRATION";
            ch.Objective = "Survive hostile hazards, sustain paddle durability, and neutralize the sector!";
            ch.CurrencyReward = 500;
            ch.UnlockedLoreId = "lore_ch1";
            ch.UnlockedCosmeticId = "";
            ch.IsBossChapter = false;

            ch.IntroDialogue = {
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Pilot, anomaly detected in Sector Zero! The grid's defense matrix has mutated into hostile hazards."
                },
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Caution: Paddle Durability is online. Hostile projectiles and debuff zones will fracture your hull! Collect Repair Kits to mend damage."
                }
            };

            ch.OutroDialogue = {
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Sector Zero secured! Paddle telemetry synchronized. The neural anomaly is retreating deeper into the core grid."
                }
            };

            chapters.push_back(ch);
        }

        // Chapter 2: The Grid Depths
        {
            StoryChapter ch;
            ch.ChapterIndex = 1;
            ch.ChapterNumberStr = "CHAPTER 02";
            ch.Title = "GRID DEPTHS: THE RESISTANCE";
            ch.Subtitle = "TEMPORARY SHIELDING PROTOCOLS";
            ch.Objective = "Withstand atmospheric debuffs and deploy energy shielding against falling debris!";
            ch.CurrencyReward = 750;
            ch.UnlockedLoreId = "lore_ch2";
            ch.UnlockedCosmeticId = "";
            ch.IsBossChapter = false;

            ch.IntroDialogue = {
                {
                    "SYSTEM AI",
                    "AUTONOMOUS NETWORK",
                    Color{255, 180, 50, 255},
                    "ALERT: Heavy grid corruption detected. Incoming hostile bombardment."
                },
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Deploy temporary shield barriers whenever possible! Severe damage will fracture the paddle and cause immediate hull breach."
                }
            };

            ch.OutroDialogue = {
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Grid depths stabilized! Sensors indicate a massive mechanical entity ahead. It's OMEGA-01, the nexus architect!"
                }
            };

            chapters.push_back(ch);
        }

        // Chapter 3: The Core Chamber - Boss Battle
        {
            StoryChapter ch;
            ch.ChapterIndex = 2;
            ch.ChapterNumberStr = "CHAPTER 03";
            ch.Title = "CORE CHAMBER: CONFRONTING OMEGA";
            ch.Subtitle = "MULTI-PHASE NEXUS SHOWDOWN";
            ch.Objective = "Strike OMEGA-01's exposed reactor core! Avoid lethal laser beams and bullet hell volleys!";
            ch.CurrencyReward = 2000;
            ch.UnlockedLoreId = "lore_ch3";
            ch.UnlockedCosmeticId = "paddle_obsidian";
            ch.IsBossChapter = true;

            ch.IntroDialogue = {
                {
                    "ARCHITECT OMEGA",
                    "AUTONOMOUS NEXUS // OVERLORD",
                    Color{255, 60, 60, 255},
                    "INTRUDER IDENTIFIED. YOU ARE A FRACTURE IN THE MATRIX. PREPARE FOR SYSTEM PURGE."
                },
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "Pilot! Its armored hull resists direct attacks. Deflect the balls straight into its glowing core reactor to deal critical damage!"
                }
            };

            ch.OutroDialogue = {
                {
                    "ARCHITECT OMEGA",
                    "AUTONOMOUS NEXUS // OVERLORD",
                    Color{255, 60, 60, 255},
                    "CRITICAL ERROR... CORE RUPTURE DETECTED... SHUTTING DOWN..."
                },
                {
                    "OPERATOR NOVA",
                    "REBREAKER TACTICAL COMMAND",
                    Color{80, 220, 255, 255},
                    "OMEGA-01 has fallen! The grid is restored! You've unlocked the legendary Void Obsidian hull and full archives. Outstanding piloting!"
                }
            };

            chapters.push_back(ch);
        }

        return chapters;
    }
};
