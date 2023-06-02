#include "pch_game.h"
#include "JLib/Arena.h"
#include "JLib/ArrayUtils.h"
#include "JLib/Math.h"
#include "JLib/VectorUtils.h"
#include "States/PlayerState.h"
#include "Utils/Shuffle.h"
#include "Library.h"
#include "Cards/RoomCard.h"
#include "States/BoardState.h"
#include "States/GameState.h"

void* Alloc(const uint32_t size)
{
	return malloc(size);
}

void Free(void* ptr)
{
	return free(ptr);
}

uint32_t RollDice(const game::Dice* dice, const uint32_t count)
{
	uint32_t result = 0;
	for (uint32_t i = 0; i < count; ++i)
	{
		const auto& die = dice[i];
		uint32_t dieSize = 0;
		switch (die.dType)
		{
			case game::Dice::Type::d4:
				dieSize = 4;
				break;
			case game::Dice::Type::d6:
				dieSize = 6;
				break;
			case game::Dice::Type::d20:
				dieSize = 20;
				break;
			default:
				std::cerr << "Die type not supported" << std::endl;
				break;
		}

		for (uint32_t j = 0; j < die.count; ++j)
			result += 1 + rand() % dieSize;
	}
	return result;
}

int ExecuteGameLoop(game::PlayerState* playerState)
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

	game::Library library{};

	library.monsters = jv::CreateArray<game::MonsterCard>(arena, 30);

	// temp.
	for (auto& monster : library.monsters)
	{
		monster = {};
		monster.tier = 1 + rand() % 7;
		monster.unique = false;
		monster.count = 1;
		monster.attack = 1;
		monster.health = 1;
	}

	// temp.
	for (int i = 0; i < game::NEW_GAME_MONSTER_SELECTION_COUNT; ++i)
		library.monsters[i].tier = 1;

	library.artifacts = jv::CreateArray<game::ArtifactCard>(arena, 30);
	for (auto& artifact : library.artifacts)
	{
		artifact = {};
		artifact.tier = 1 + rand() % 7;
		artifact.unique = false;
	}

	// temp.
	for (int i = 0; i < game::NEW_GAME_ARTIFACT_SELECTION_COUNT; ++i)
	{
		library.artifacts[i].tier = 1;
	}

	library.quests = jv::CreateArray<game::QuestCard>(arena, 30);
	for (auto& quest : library.quests)
	{
		quest = {};
		quest.tier = rand() % 8;
		quest.minRoomCount = 2;
		quest.roomCountDice = {};
		quest.roomCountDice.dType = game::Dice::Type::d4;
	}

	// temp.
	for (int i = 0; i < game::NEW_RUN_QUEST_SELECTION_COUNT; ++i)
	{
		library.quests[i].tier = 1;
	}

	// Select dungeon tier.
	uint32_t tier = 0;
	while (true)
	{
		std::cout << "Select your dungeon tier: 1-" << game::MAX_TIER << std::endl;
		std::cin >> tier;
		if (tier > 0 && tier <= game::MAX_TIER)
			break;
		std::cout << "Invalid tier selected." << std::endl;
	}
	std::cout << "tier " << tier << " selected." << std::endl;

	// Set up monster deck.
	uint32_t monsterMaxDeckSize = 0;
	for (uint32_t i = 0; i < library.monsters.length; ++i)
	{
		const auto& monster = library.monsters[i];
		if (monster.tier > tier)
			continue;
		if (monster.unique)
			continue;
		monsterMaxDeckSize += monster.count;
	}

	auto monsterIds = jv::CreateVector<uint32_t>(arena, library.monsters.length);
	for (uint32_t i = 0; i < library.monsters.length; ++i)
	{
		const auto& monster = library.monsters[i];
		if (monster.tier > tier)
			continue;
		if (monster.unique)
			continue;
		monsterIds.Add() = i;
	}
	std::cout << "Finished setting up monster deck." << std::endl;

	// Set up artifact deck.
	auto artifactIds = jv::CreateVector<uint32_t>(arena, library.artifacts.length);
	for (uint32_t i = 0; i < library.artifacts.length; ++i)
	{
		const auto& artifact = library.artifacts[i];
		if (artifact.tier > tier)
			continue;
		if (artifact.unique)
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

		const auto monsterChoices = jv::CreateArray<uint32_t>(tempArena, game::NEW_GAME_MONSTER_SELECTION_COUNT);
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

		const auto artifactChoices = jv::CreateArray<uint32_t>(tempArena, game::NEW_GAME_ARTIFACT_SELECTION_COUNT);
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

		// Shuffle decks again.
		Shuffle(monsterIds.ptr, monsterIds.count);
		Shuffle(artifactIds.ptr, artifactIds.count);
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
			std::cout << "Artifact Id: " << playerState->artifactsIds[j + i * game::MAX_ARTIFACTS_PER_CHARACTER] << std::endl;
		}
	}

	uint32_t questId;
	{
		const auto tempScope = tempArena.CreateScope();
		uint32_t questChoice = 0;
		// Set up monster deck.
		auto questIds = jv::CreateVector<uint32_t>(arena, library.monsters.length);
		for (uint32_t i = 0; i < library.quests.length; ++i)
		{
			const auto& quest = library.quests[i];
			if (quest.tier > tier)
				continue;
			questIds.Add() = i;
		}

		Shuffle(questIds.ptr, questIds.count);

		std::cout << "Finished setting up quest deck." << std::endl;

		const auto questSelectionCount = jv::Min<uint32_t>(game::NEW_RUN_QUEST_SELECTION_COUNT, questIds.count);
		assert(questSelectionCount > 0);
		
		while (true)
		{
			std::cout << "Choose one of these quests to pursue:" << std::endl;

			for (uint32_t i = 0; i < questSelectionCount; ++i)
				std::cout << i << "Quest: " << questIds[i] << std::endl;

			std::cin >> questChoice;
			if (questChoice <= questSelectionCount)
				break;

			std::cout << "Invalid quest selected." << std::endl;
		}

		questId = questIds[questChoice];
		std::cout << "quest " << questId << " selected." << std::endl;

		tempArena.DestroyScope(tempScope);
	}
	const auto& quest = library.quests[questId];

	library.rooms = jv::CreateArray<game::RoomCard>(arena, 30);

	// temp.
	for (auto& room : library.rooms)
	{
		room = {};
	}

	// Decide on the amount of rooms.
	const uint32_t roomCount = quest.minRoomCount + RollDice(&quest.roomCountDice, 1);
	auto roomIds = jv::CreateVector<uint32_t>(arena, library.rooms.length);
	roomIds.count = roomIds.length;
	for (uint32_t i = 0; i < roomIds.length; ++i)
		roomIds[i] = i;
	Shuffle(roomIds.ptr, roomIds.length);

	game::GameState gameState{};
	gameState.playerState = playerState;
	gameState.library = &library;

	bool quit = false;
	while(!quit)
	{
		game::BoardState boardState{};
		bool isPartyWiped = false;

		uint32_t roomsRemaining = roomCount;
		while(roomsRemaining-- > 0)
		{
			uint32_t roomChoice;
			uint32_t roomId;
			while (true)
			{
				std::cout << "Choose one of these rooms to enter:" << std::endl;

				for (uint32_t i = 0; i < game::ROOM_SELECTION_COUNT; ++i)
					std::cout << i << ": " << roomIds[i] << std::endl;

				std::cin >> roomChoice;
				if (roomChoice <= game::ROOM_SELECTION_COUNT)
					break;

				std::cout << "Invalid room selected." << std::endl;
			}

			roomId = roomIds[roomChoice];
			std::cout << "room " << roomId << " selected." << std::endl;
			roomIds.RemoveAt(roomChoice);
			std::cout << "Entering room " << roomId << "." << std::endl;
			Shuffle(roomIds.ptr, roomIds.count);

			uint32_t roomRating = 0;
			boardState.enemyMonsterCount = 0;
			Shuffle(monsterIds.ptr, monsterIds.count);

			// If it's the final room, fight the boss.
			if(roomsRemaining == 0)
			{
				boardState.enemyMonsterCount = 1;
				boardState.enemyMonsterIds[0] = quest.bossId;
			}
			else
				while (roomRating < tier)
				{
					uint32_t monsterId = monsterIds.Pop();
					const auto& monster = library.monsters[monsterId];
					roomRating += monster.tier;
					boardState.enemyMonsterIds[boardState.enemyMonsterCount++] = monsterId;
					std::cout << "Monster " << monsterId << " encountered!" << std::endl;
				}
			// Reset monster states.
			for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
				boardState.monsterStates[game::MAX_PARTY_SIZE + i] = {};

			const uint32_t startingEnemyMonsterCount = boardState.enemyMonsterCount;

			game::Dice d4;
			d4.dType = game::Dice::Type::d4;

			// Start taking turns.
			while(boardState.enemyMonsterCount > 0)
			{
				bool livingPartyMembers[game::MAX_PARTY_SIZE]{};
				
				isPartyWiped = true;
				for (uint32_t i = 0; i < playerState->partySize; ++i)
				{
					const auto& monster = library.monsters[playerState->partyIds[i]];
					livingPartyMembers[i] = monster.health > boardState.monsterStates[i].damageTaken;
					isPartyWiped = !isPartyWiped ? false : !livingPartyMembers[i];
				}

				if(isPartyWiped)
				{
					std::cout << "Your party has been wiped!" << std::endl;
					break;
				}

				// Roll intention.
				for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
				{
					uint32_t intention;
					do
						intention = RollDice(&d4, 1) - 1;
					while (!livingPartyMembers[intention]);

					boardState.enemyMonsterIntentions[i] = intention;
					std::cout << "Monster " << i << " intention rolled: " << intention << std::endl;
				}

				// Player turn.
				for (uint32_t i = 0; i < playerState->partySize; ++i)
				{
					const auto& playerMonster = library.monsters[playerState->partyIds[i]];
					if (!livingPartyMembers[i])
						continue;

					uint32_t intentionChoice;
					while (true)
					{
						std::cout << "Choose whom to attack: 0-" << boardState.enemyMonsterCount - 1 << std::endl;
						
						std::cin >> intentionChoice;
						if (intentionChoice < boardState.enemyMonsterCount)
							break;

						std::cout << "Invalid target selected." << std::endl;
					}

					const uint32_t monsterId = boardState.enemyMonsterIds[intentionChoice];
					const auto& monster = library.monsters[monsterId];

					std::cout << "Attacked monster at slot " << intentionChoice << "for " << playerMonster.attack << " damage." << std::endl;

					auto& damageTaken = boardState.monsterStates[game::MAX_PARTY_SIZE + intentionChoice].damageTaken;
					damageTaken += playerMonster.attack;

					{
						uint32_t intention;
						do
							intention = RollDice(&d4, 1) - 1;
						while (!livingPartyMembers[intention]);

						boardState.enemyMonsterIntentions[intentionChoice] = intention;
						std::cout << "Monster " << intentionChoice << " intention rolled: " << intention << std::endl;
					}

					if(monster.health <= damageTaken)
					{
						std::cout << "Monster at slot " << intentionChoice << " died!" << std::endl;

						// Remove monster from the board.
						for (uint32_t j = intentionChoice; j < boardState.enemyMonsterCount; ++j)
						{
							boardState.monsterStates[game::MAX_PARTY_SIZE + j] = boardState.monsterStates[game::MAX_PARTY_SIZE + j + 1];
							boardState.enemyMonsterIds[j] = boardState.enemyMonsterIds[j + 1];
						}

						boardState.enemyMonsterCount--;
					}
				}

				// Monster turn.
				for (uint32_t i = 0; i < boardState.enemyMonsterCount; ++i)
				{
					const uint32_t monsterId = boardState.enemyMonsterIds[i];
					const auto& monster = library.monsters[monsterId];

					const uint32_t intention = boardState.enemyMonsterIntentions[i];
					const uint32_t playerMonsterId = playerState->partyIds[intention];

					std::cout << "Monster attacks at slot " << intention << "for " << monster.attack << " damage." << std::endl;

					auto& damageTaken = boardState.monsterStates[intention].damageTaken;
					damageTaken += monster.attack;

					const auto& playerMonster = library.monsters[playerMonsterId];
					if(damageTaken > playerMonster.health)
						std::cout << "Player monster defeated at slot " << intention << "." << std::endl;
				}
			}

			// Add the monsters back into the deck.
			for (uint32_t i = 0; i < startingEnemyMonsterCount; ++i)
				monsterIds.Add() = boardState.enemyMonsterIds[i];

			if(isPartyWiped)
				break;

			std::cout << "Cleared room " << roomId << "." << std::endl;
		}

		if(!isPartyWiped)
			std::cout << "Cleared quest " << questId << "." << std::endl;
		else
			std::cout << "Failed quest " << questId << "." << std::endl;

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
	game::PlayerState state{};

	int ret = 0;
	while(!ret)
		ret = ExecuteGameLoop(&state);
	return 0;
}