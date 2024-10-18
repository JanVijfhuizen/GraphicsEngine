#include "pch_game.h"
#include "Levels/MainMenuLevel.h"
#include <Utils/SubTextureUtils.h>

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		inTutorial = false;
		inResolutionSelect = false;
		inCredits = false;
		openTime = 0;
		changingResolution = false;
	}

	void MainMenuLevel::DrawTitle(const LevelUpdateInfo& info)
	{
		const auto& titleAtlasTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::title)];
		jv::ge::SubTexture titleFrames[4];
		Divide(titleAtlasTexture.subTexture, titleFrames, 4);

		uint32_t index = floor(fmodf(GetTime() * 6, 4));

		PixelPerfectRenderTask titleTask;
		titleTask.position = glm::ivec2(-6, SIMULATED_RESOLUTION.y - 64);
		titleTask.scale = titleAtlasTexture.resolution / glm::ivec2(4, 1);
		titleTask.subTexture = titleFrames[index];
		info.renderTasks.Push(titleTask);
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;
		if (changingResolution)
			return true;

		openTime += info.deltaTime;

		constexpr glm::ivec2 origin = { 18, SIMULATED_RESOLUTION.y - 36 };
		constexpr auto buttonOrigin = origin - glm::ivec2(-4, 36);
		constexpr uint32_t BUTTON_OFFSET = 20;
		constexpr uint32_t SMALL_BUTTON_OFFSET = 12;

		if (inCredits)
		{
			DrawTitle(info);

			TextTask textTask{};
			textTask.position = buttonOrigin + glm::ivec2(0, 3);;
			textTask.text = "jan vijfhuizen - most stuff";
			textTask.lifetime = openTime;
			info.textTasks.Push(textTask);

			textTask.position -= glm::ivec2(0, SMALL_BUTTON_OFFSET);
			textTask.text = "tristan ten cate - audio";
			info.textTasks.Push(textTask);

			textTask.position -= glm::ivec2(0, SMALL_BUTTON_OFFSET);
			textTask.text = "dragos popescu - programming help";
			info.textTasks.Push(textTask);

			textTask.position -= glm::ivec2(0, SMALL_BUTTON_OFFSET);
			textTask.text = "ana dirica - art help";
			info.textTasks.Push(textTask);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.width = 140;
			buttonDrawInfo.drawLineByDefault = false;
			buttonDrawInfo.origin = textTask.position;
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 2;
			buttonDrawInfo.text = "back";
			buttonDrawInfo.drawLineByDefault = true;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				inCredits = false;
				openTime = 0;
			}
				
			return true;
		}

		if (inResolutionSelect)
		{
			HeaderDrawInfo headerDrawInfo{};
			headerDrawInfo.scale = 1;
			headerDrawInfo.origin = origin;
			headerDrawInfo.text = "you can set a custom resolution by changing the resolution text file that comes with the game. the game is made with a specific resolution in mind so graphical errors may occur if a custom resolution is set.";
			headerDrawInfo.overrideLifeTime = 99;
			headerDrawInfo.border = true;
			DrawHeader(info, headerDrawInfo);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.width = 140;
			buttonDrawInfo.drawLineByDefault = false;

			buttonDrawInfo.text = "320x240";
			buttonDrawInfo.origin = origin - glm::ivec2(0, SMALL_BUTTON_OFFSET * 8);
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				info.requestedResolution = glm::ivec2(320, 240);
				changingResolution = true;
				return true;
			}

			buttonDrawInfo.text = "640x480";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				info.requestedResolution = glm::ivec2(640, 480);
				changingResolution = true;
				return true;
			}

			buttonDrawInfo.text = "960x720 - recommended";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				info.requestedResolution = glm::ivec2(960, 720);
				changingResolution = true;
				return true;
			}

			buttonDrawInfo.text = "1280x720";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				info.requestedResolution = glm::ivec2(1280, 720);
				changingResolution = true;
				return true;
			}

			buttonDrawInfo.text = "1920x1080";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				info.requestedResolution = glm::ivec2(1920, 1080);
				changingResolution = true;
				return true;
			}

			if(!info.isFullScreen)
			{
				buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
				buttonDrawInfo.text = "to fullscreen";
				if (DrawButton(info, buttonDrawInfo, openTime))
				{
					info.isFullScreen = true;
					changingResolution = true;
				}
			}

			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 2;
			buttonDrawInfo.text = "back";
			buttonDrawInfo.drawLineByDefault = true;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				inResolutionSelect = false;
				openTime = 0;
			}
				
			return true;
		}

		if(inTutorial)
		{
			HeaderDrawInfo headerDrawInfo{};
			headerDrawInfo.origin = origin;
			headerDrawInfo.scale = 1;
			headerDrawInfo.text = "you start out with some monsters and a deck of spells. your goal is to get as far as possible while collecting new monsters, spells and artifacts along the way.";
			headerDrawInfo.overrideLifeTime = 99;
			headerDrawInfo.border = true;
			DrawHeader(info, headerDrawInfo);

			TextTask textTask{};
			textTask.position = origin - glm::ivec2(0, SMALL_BUTTON_OFFSET * 6);
			textTask.text = "mouse left - select and drag cards.";
			textTask.lifetime = openTime;
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "mouse right - look at the card text.";
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "space - end turn.";
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "esc - open menu.";
			info.textTasks.Push(textTask);

			headerDrawInfo.text = "drag a monster to an enemy monster to attack it. drag a spell to cast it.";
			headerDrawInfo.origin = textTask.position;
			headerDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 2;
			headerDrawInfo.overrideLifeTime = openTime;
			DrawHeader(info, headerDrawInfo);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.origin = textTask.position;
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 6;
			buttonDrawInfo.text = "back";
			buttonDrawInfo.width = 96;
			if (DrawButton(info, buttonDrawInfo, openTime))
			{
				inTutorial = false;
				openTime = 0;
			}
				
			return true;
		}

		DrawTitle(info);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = buttonOrigin;
		buttonDrawInfo.text = "start";
		buttonDrawInfo.width = 140;
		buttonDrawInfo.drawLineByDefault = false;
		if (DrawButton(info, buttonDrawInfo, openTime))
		{
			Load(LevelIndex::newGame, true);
			return true;
		}

		buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
		buttonDrawInfo.text = "change resolution";
		if (DrawButton(info, buttonDrawInfo, openTime))
		{
			inResolutionSelect = true;
			openTime = 0;
			return true;
		}

		buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
		buttonDrawInfo.text = "how to play";
		if (DrawButton(info, buttonDrawInfo, openTime))
		{
			inTutorial = true;
			openTime = 0;
			return true;
		}

		buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
		buttonDrawInfo.text = "credits";
		if (DrawButton(info, buttonDrawInfo, openTime))
		{
			inCredits = true;
			openTime = 0;
			return true;
		}

		buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
		buttonDrawInfo.text = "exit";
		if (DrawButton(info, buttonDrawInfo, openTime))
			return false;
		return true;
	}
}
