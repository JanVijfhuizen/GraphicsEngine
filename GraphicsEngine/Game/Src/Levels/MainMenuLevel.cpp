#include "pch_game.h"
#include "Levels/MainMenuLevel.h"

namespace game
{
	void MainMenuLevel::Create(const LevelCreateInfo& info)
	{
		Level::Create(info);
		inTutorial = false;
		inResolutionSelect = false;
	}

	bool MainMenuLevel::Update(const LevelUpdateInfo& info, LevelIndex& loadLevelIndex)
	{
		if (!Level::Update(info, loadLevelIndex))
			return false;

		constexpr glm::ivec2 origin = { 18, SIMULATED_RESOLUTION.y - 36 };
		constexpr auto buttonOrigin = origin - glm::ivec2(-4, 36);
		constexpr uint32_t BUTTON_OFFSET = 20;
		constexpr uint32_t SMALL_BUTTON_OFFSET = 12;

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
			if (DrawButton(info, buttonDrawInfo))
			{
				info.requestedResolution = glm::ivec2(320, 240);
				return true;
			}

			buttonDrawInfo.text = "640x480";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo))
			{
				info.requestedResolution = glm::ivec2(640, 480);
				return true;
			}

			buttonDrawInfo.text = "960x720 - recommended";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo))
			{
				info.requestedResolution = glm::ivec2(960, 720);
				return true;
			}

			buttonDrawInfo.text = "1280x720";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo))
			{
				info.requestedResolution = glm::ivec2(1280, 720);
				return true;
			}

			buttonDrawInfo.text = "1920x1080";
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
			if (DrawButton(info, buttonDrawInfo))
			{
				info.requestedResolution = glm::ivec2(1920, 1080);
				return true;
			}

			if(!info.isFullScreen)
			{
				buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET;
				buttonDrawInfo.text = "to fullscreen";
				if (DrawButton(info, buttonDrawInfo))
					info.isFullScreen = true;
			}

			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 2;
			buttonDrawInfo.text = "back";
			buttonDrawInfo.drawLineByDefault = true;
			if (DrawButton(info, buttonDrawInfo))
				inResolutionSelect = false;
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
			textTask.text = "[mouse left] select and drag cards.";
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "[mouse right] look at the card text.";
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "[space] end turn.";
			info.textTasks.Push(textTask);

			textTask.position.y -= SMALL_BUTTON_OFFSET;
			textTask.text = "[esc] open menu.";
			info.textTasks.Push(textTask);

			headerDrawInfo.text = "drag a monster to an enemy monster to attack it. drag a spell to cast it.";
			headerDrawInfo.origin = textTask.position;
			headerDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 2;
			DrawHeader(info, headerDrawInfo);

			ButtonDrawInfo buttonDrawInfo{};
			buttonDrawInfo.origin = textTask.position;
			buttonDrawInfo.origin.y -= SMALL_BUTTON_OFFSET * 6;
			buttonDrawInfo.text = "back";
			buttonDrawInfo.width = 96;
			if (DrawButton(info, buttonDrawInfo))
				inTutorial = false;
			return true;
		}

		const auto& titleAtlasTexture = info.atlasTextures[static_cast<uint32_t>(TextureId::title)];

		PixelPerfectRenderTask titleTask;
		titleTask.position = glm::ivec2(0, SIMULATED_RESOLUTION.y - titleAtlasTexture.resolution.y);
		titleTask.scale = titleAtlasTexture.resolution;
		titleTask.subTexture = titleAtlasTexture.subTexture;
		//titleTask.xCenter = true;
		//titleTask.yCenter = true;
		info.renderTasks.Push(titleTask);
		
		HeaderDrawInfo headerDrawInfo{};
		headerDrawInfo.origin = origin;
		headerDrawInfo.text = "untitled card game";
		headerDrawInfo.lineLength = 10;
		//DrawHeader(info, headerDrawInfo);

		ButtonDrawInfo buttonDrawInfo{};
		buttonDrawInfo.origin = buttonOrigin;
		buttonDrawInfo.text = "start";
		buttonDrawInfo.width = 140;
		buttonDrawInfo.largeFont = true;
		buttonDrawInfo.drawLineByDefault = false;
		if (DrawButton(info, buttonDrawInfo))
			Load(LevelIndex::newGame, true);

		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = "change resolution";
		if (DrawButton(info, buttonDrawInfo))
			inResolutionSelect = true;

		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = "how to play";
		if (DrawButton(info, buttonDrawInfo))
		{
			inTutorial = true;
			return true;
		}

		buttonDrawInfo.origin.y -= BUTTON_OFFSET;
		buttonDrawInfo.text = "exit";
		if (DrawButton(info, buttonDrawInfo))
			return false;
		return true;
	}
}
