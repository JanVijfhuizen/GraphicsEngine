#include "pch.h"
#include "Shuffle.h"

#include "JLib/Arena.h"
#include "JLib/ArrayUtils.h"
#include "JLib/VectorUtils.h"

#define MAX_PARTY_SIZE 4
#define MAX_ARTIFACTS_PER_CHARACTER 4
#define MAX_TIER 7
#define NEW_GAME_MONSTER_SELECTION_COUNT 3
#define NEW_GAME_ARTIFACT_SELECTION_COUNT 4

struct PlayerState final
{
	uint32_t partySize = 0;
	uint32_t partyIds[MAX_PARTY_SIZE]{};
	uint32_t artifactsIds[MAX_PARTY_SIZE * MAX_ARTIFACTS_PER_CHARACTER]{};
	uint32_t artifactCounts[MAX_PARTY_SIZE]{};
};

struct GameState final
{
	PlayerState* playerState;
	uint32_t partyHealth[MAX_PARTY_SIZE];
};

struct MonsterCard final
{
	uint32_t tier;
};

struct ArtifactCard final
{
	uint32_t tier;
};

void* Alloc(const uint32_t size)
{
	return malloc(size);
}

void Free(void* ptr)
{
	return free(ptr);
}

int ExecuteGameLoop(PlayerState* playerState)
{
	srand(time(nullptr));

	constexpr uint32_t arenaSize = 4096;
	const auto arenaMem = malloc(arenaSize);
	const auto tempArenaMem = malloc(arenaSize);
	const auto frameArenaMem = malloc(arenaSize);

	jv::ArenaCreateInfo arenaInfo{};
	arenaInfo.alloc = Alloc;
	arenaInfo.free = Free;
	arenaInfo.memory = arenaMem;
	arenaInfo.memorySize = arenaSize;
	auto arena = jv::Arena::Create(arenaInfo);
	arenaInfo.memory = tempArenaMem;
	auto tempArena = jv::Arena::Create(arenaInfo);
	arenaInfo.memory = frameArenaMem;
	auto frameArena = jv::Arena::Create(arenaInfo);

	const auto monsters = jv::CreateArray<MonsterCard>(arena, 30);
	for (auto& monster : monsters)
	{
		monster.tier = rand() % 8;
	}
	const auto artifacts = jv::CreateArray<ArtifactCard>(arena, 30);
	for (auto& artifact : artifacts)
	{
		artifact.tier = rand() % 8;
	}

	// Select dungeon tier.
	uint32_t tier = 0;
	while (true)
	{
		std::cout << "Select your dungeon tier: 1-" << MAX_TIER << std::endl;
		std::cin >> tier;
		if (tier > 0 && tier <= MAX_TIER)
			break;
		std::cout << "Invalid tier selected." << std::endl;
	}
	std::cout << "tier " << tier << " selected." << std::endl;

	// Set up monster deck.
	auto monsterIds = jv::CreateVector<uint32_t>(arena, monsters.length);
	for (uint32_t i = 0; i < monsters.length; ++i)
	{
		const auto& monster = monsters[i];
		if (monster.tier > tier)
			continue;
		monsterIds.Add() = i;
	}
	std::cout << "Finished setting up monster deck." << std::endl;

	// Set up artifact deck.
	auto artifactIds = jv::CreateVector<uint32_t>(arena, artifacts.length);
	for (uint32_t i = 0; i < artifacts.length; ++i)
	{
		const auto& artifact = artifacts[i];
		if (artifact.tier > tier)
			continue;
		artifactIds.Add() = i;
	}
	std::cout << "Finished setting up artifact deck." << std::endl;

	Shuffle(monsterIds.ptr, monsterIds.count);
	std::cout << "Shuffled monster deck." << std::endl;
	Shuffle(artifactIds.ptr, artifactIds.count);
	std::cout << "Shuffled artifact deck." << std::endl;

	// If it's a new party.
	if(playerState->partySize == 0)
	{
		std::cout << "Create a new party." << std::endl;

		const auto tempScope = tempArena.CreateScope();

		const auto monsterChoices = jv::CreateArray<uint32_t>(tempArena, NEW_GAME_MONSTER_SELECTION_COUNT);
		for (auto& choice : monsterChoices)
			choice = monsterIds.Pop();

		uint32_t monsterChoice = 0;
		while(true)
		{
			std::cout << "Choose one of these monsters to start with:" << std::endl;
			for (uint32_t i = 0; i < monsterChoices.length; ++i)
			{
				std::cout << i << ": " << monsterChoices[i] << std::endl;
			}

			std::cin >> monsterChoice;
			if (monsterChoice <= monsterChoices.length)
				break;

			std::cout << "Invalid monster selected." << std::endl;
		}

		std::cout << "monster " << monsterChoices[monsterChoice] << " selected." << std::endl;

		// Put others back into the deck.
		for (uint32_t i = 0; i < monsterChoices.length; ++i)
		{
			if (monsterChoice == i)
				continue;
			monsterIds.Add() = monsterChoices[i];
		}

		const auto artifactChoices = jv::CreateArray<uint32_t>(tempArena, NEW_GAME_ARTIFACT_SELECTION_COUNT);
		for (auto& choice : artifactChoices)
			choice = artifactIds.Pop();

		uint32_t artifactChoice = 0;
		while (true)
		{
			std::cout << "Choose one of these artifacts to start with:" << std::endl;
			for (uint32_t i = 0; i < artifactChoices.length; ++i)
			{
				std::cout << i << ": " << artifactChoices[i] << std::endl;
			}

			std::cin >> artifactChoice;
			if (artifactChoice <= artifactChoices.length)
				break;

			std::cout << "Invalid artifact selected." << std::endl;
		}

		std::cout << "artifact " << artifactChoices[artifactChoice] << " selected." << std::endl;

		// Put others back into the deck.
		for (uint32_t i = 0; i < artifactChoices.length; ++i)
		{
			if (artifactChoice == i)
				continue;
			artifactIds.Add() = artifactChoices[i];
		}

		playerState->partyIds[0] = monsterChoices[monsterChoice];
		playerState->artifactsIds[0] = artifactChoices[artifactChoice];
		playerState->partySize = 1;
		playerState->artifactCounts[0] = 1;

		tempArena.DestroyScope(tempScope);
	}

	std::cout << "Current party: " << std::endl;
	for (uint32_t i = 0; i < playerState->partySize; ++i)
	{
		const uint32_t monsterId = playerState->partyIds[i];
		std::cout << "Party No." << i << ": " << std::endl;
		std::cout << "Monster Id: " << monsterId << std::endl;

		const uint32_t artifactCount = playerState->artifactCounts[i];
		for (uint32_t j = 0; j < artifactCount; ++j)
		{
			std::cout << "Artifact Id: " << playerState->artifactsIds[j + i * MAX_ARTIFACTS_PER_CHARACTER] << std::endl;
		}
	}

	bool quit = false;
	while(!quit)
	{
		frameArena.Clear();
		quit = true;
	}

	jv::Arena::Destroy(frameArena);
	jv::Arena::Destroy(tempArena);
	jv::Arena::Destroy(arena);
	return 0;
}

int main()
{
	PlayerState state{};
	return ExecuteGameLoop(&state);
}