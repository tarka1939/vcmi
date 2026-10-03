/*
 * RemoteDwellingsWindow.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "RemoteDwellingsWindow.h"

#include "GUIClasses.h"

#include "../CPlayerInterface.h"
#include "../GameEngine.h"
#include "../GameInstance.h"
#include "../gui/Shortcut.h"
#include "../gui/WindowHandler.h"
#include "../media/ISoundPlayer.h"
#include "../widgets/Buttons.h"
#include "../widgets/CComponent.h"
#include "../widgets/GraphicalPrimitiveCanvas.h"
#include "../widgets/Images.h"
#include "../widgets/ObjectLists.h"
#include "../widgets/TextControls.h"

#include "../../lib/CCreatureHandler.h"
#include "../../lib/CSoundBase.h"
#include "../../lib/GameLibrary.h"
#include "../../lib/callback/CCallback.h"
#include "../../lib/mapObjects/CGTownInstance.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../../lib/texts/MetaString.h"

namespace
{
constexpr int LIST_X = 20;
constexpr int LIST_Y = 50;
constexpr int ROW_WIDTH = 360;
constexpr int ROW_HEIGHT = 38;
constexpr int ROW_OFFSET = 40;
constexpr int VISIBLE_ROWS = 8;
constexpr int SLIDER_WIDTH = 16;
constexpr int ICON_STEP = 36;
constexpr size_t MAX_ICONS = 4;
}

RemoteDwellingsWindow::CItem::CItem(RemoteDwellingsWindow * parent, const Entry & entry)
	: parent(parent)
	, dwelling(entry.dwelling)
	, enabled(entry.available > 0)
{
	OBJECT_CONSTRUCTION;

	pos.w = ROW_WIDTH;
	pos.h = ROW_HEIGHT;
	if(enabled)
		addUsedEvents(LCLICK);

	const ColorRGBA fillColor = enabled ? ColorRGBA(0, 0, 0, 75) : ColorRGBA(0, 0, 0, 150);
	const ColorRGBA borderColor = enabled ? ColorRGBA(128, 100, 75) : ColorRGBA(64, 50, 38);
	const ColorRGBA textColor = enabled ? Colors::WHITE : ColorRGBA(158, 130, 105);
	background = std::make_shared<TransparentFilledRectangle>(Rect(0, 0, pos.w, pos.h), fillColor, borderColor);

	const size_t iconsCount = std::min(entry.levels.size(), MAX_ICONS);
	const int iconsX = pos.w - 4 - static_cast<int>(iconsCount) * ICON_STEP;
	for(size_t i = 0; i < iconsCount; i++)
	{
		const auto & level = dwelling->creatures.at(entry.levels[i]);
		const int x = iconsX + static_cast<int>(i) * ICON_STEP;
		icons.push_back(std::make_shared<CAnimImage>(AnimationPath::builtin("CPRSMALL"), level.second.front().toCreature()->getIconIndex(), 0, x, 3));
		amounts.push_back(std::make_shared<CLabel>(x + 32, 35, FONT_TINY, ETextAlignment::BOTTOMRIGHT, textColor, std::to_string(level.first)));
	}

	name = std::make_shared<CLabel>(8, pos.h / 2, FONT_SMALL, ETextAlignment::CENTERLEFT, textColor, entry.name, iconsX - 12);
}

void RemoteDwellingsWindow::CItem::clickPressed(const Point & cursorPosition)
{
	ENGINE->sound().playSound(soundBase::button);
	parent->openRecruitment(dwelling);
}

RemoteDwellingsWindow::RemoteDwellingsWindow(const CGTownInstance * town)
	: CWindowObject(BORDERED)
	, town(town)
{
	OBJECT_CONSTRUCTION;

	pos.w = LIST_X * 2 + ROW_WIDTH + 4 + SLIDER_WIDTH;
	pos.h = LIST_Y + VISIBLE_ROWS * ROW_OFFSET + 55;

	background = std::make_shared<FilledTexturePlayerColored>(Rect(0, 0, pos.w, pos.h));
	background->setPlayerColor(GAME->interface()->playerID);

	title = std::make_shared<CLabel>(pos.w / 2, 25, FONT_BIG, ETextAlignment::CENTER, Colors::YELLOW, LIBRARY->generaltexth->translate("vcmi.townWindow.remoteDwellings.title"));
	emptyListLabel = std::make_shared<CLabel>(pos.w / 2, LIST_Y + VISIBLE_ROWS * ROW_OFFSET / 2, FONT_SMALL, ETextAlignment::CENTER, Colors::WHITE, LIBRARY->generaltexth->translate("vcmi.townWindow.remoteDwellings.empty"));

	const int listHeight = VISIBLE_ROWS * ROW_OFFSET - (ROW_OFFSET - ROW_HEIGHT);
	list = std::make_shared<CListBox>([this](size_t index){ return createItem(index); }, Point(LIST_X, LIST_Y), Point(0, ROW_OFFSET), VISIBLE_ROWS, 0, 0, 1, Rect(ROW_WIDTH + 4, 0, listHeight, listHeight));
	list->setRedrawParent(true);

	closeButton = std::make_shared<CButton>(Point(pos.w / 2 - 32, pos.h - 45), AnimationPath::builtin("ICN6432.DEF"), CButton::tooltip(), [this](){ close(); }, EShortcut::GLOBAL_CANCEL);

	updateEntries();
	center();
}

std::vector<size_t> RemoteDwellingsWindow::getRecruitableLevels(const CGDwelling * dwelling) const
{
	std::vector<size_t> result;
	for(size_t level = 0; level < dwelling->creatures.size(); level++)
	{
		const auto & creatures = dwelling->creatures[level].second;
		if(!creatures.empty() && dwelling->canRecruitRemotely(GAME->interface()->playerID, town->getUpperArmy(), creatures.front()))
			result.push_back(level);
	}
	return result;
}

bool RemoteDwellingsWindow::compareEntries(const Entry & a, const Entry & b)
{
	// dwellings with several levels (e.g. Golem Factory) are sorted by their lowest creature level
	const auto lowestLevel = [](const Entry & entry)
	{
		int result = std::numeric_limits<int>::max();
		for(size_t level : entry.levels)
			vstd::amin(result, entry.dwelling->creatures[level].second.front().toCreature()->getLevel());
		return result;
	};

	const int levelA = lowestLevel(a);
	const int levelB = lowestLevel(b);
	// object id keeps dwellings with identical names in the same order every time the list is refreshed
	return std::tie(levelA, a.name, a.dwelling->id) < std::tie(levelB, b.name, b.dwelling->id);
}

void RemoteDwellingsWindow::updateEntries()
{
	entries.clear();

	for(const auto * object : GAME->interface()->cb->getMyObjects())
	{
		const auto * dwelling = dynamic_cast<const CGDwelling *>(object);
		if(!dwelling)
			continue;

		Entry entry{dwelling, dwelling->getObjectName(), getRecruitableLevels(dwelling), 0};
		if(entry.levels.empty())
			continue;

		for(size_t level : entry.levels)
			entry.available += dwelling->creatures[level].first;

		entries.push_back(entry);
	}

	std::sort(entries.begin(), entries.end(), &RemoteDwellingsWindow::compareEntries);

	emptyListLabel->setEnabled(entries.empty());
	list->resize(entries.size());
	list->reset();
	redraw();
}

std::shared_ptr<CIntObject> RemoteDwellingsWindow::createItem(size_t index)
{
	if(index < entries.size())
		return std::make_shared<CItem>(this, entries[index]);
	return std::shared_ptr<CIntObject>();
}

void RemoteDwellingsWindow::openRecruitment(const CGDwelling * dwelling)
{
	const CArmedInstance * dst = town->getUpperArmy();

	if(dwelling->creaturesJoinForFree())
	{
		// like for a hero visiting the dwelling, all creatures join for free without the recruitment window
		const CreatureID creature = dwelling->creatures[0].second[0];
		const int amount = static_cast<int>(dwelling->creatures[0].first);
		if(!dst->getSlotFor(creature).validSlot())
		{
			GAME->interface()->showInfoDialog(LIBRARY->generaltexth->allTexts[17]); //There is no room in the garrison for this army.
			return;
		}

		MetaString text = MetaString::createFromTextID("core.advevent.35"); //{%s} Would you like to recruit %s?
		text.replaceRawString(dwelling->getObjectName());
		text.replaceNamePlural(creature);
		auto joinCb = [dwelling, dst, creature, amount]()
		{
			GAME->interface()->cb->recruitCreatures(dwelling, dst, creature, amount, 0);
		};
		GAME->interface()->showYesNoDialog(text.toString(), joinCb, nullptr, {std::make_shared<CComponent>(ComponentType::CREATURE, creature, amount)});
		return;
	}

	// same level selection as for a hero visiting the dwelling, see CGDwelling::heroAcceptsCreatures
	const int level = dwelling->ID == Obj::CREATURE_GENERATOR1 ? 0 : -1;

	auto recruitCb = [dwelling, dst](CreatureID id, int count)
	{
		GAME->interface()->cb->recruitCreatures(dwelling, dst, id, count, -1);
	};
	// no selectionMade on close: there is no server query to answer, and the list is refreshed by CPlayerInterface::availableCreaturesChanged
	ENGINE->windows().createAndPushWindow<CRecruitmentWindow>(dwelling, level, dst, recruitCb, nullptr);
}
