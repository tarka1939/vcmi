/*
 * RemoteDwellingsWindow.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "../windows/CWindowObject.h"

VCMI_LIB_NAMESPACE_BEGIN
class CGDwelling;
class CGTownInstance;
VCMI_LIB_NAMESPACE_END

class CButton;
class CLabel;
class CListBox;
class CAnimImage;
class FilledTexturePlayerColored;
class TransparentFilledRectangle;

/// Lists owned non-town dwellings and recruits from them into the garrison of the given town
class RemoteDwellingsWindow : public CWindowObject
{
	struct Entry
	{
		const CGDwelling * dwelling;
		std::string name;
		std::vector<size_t> levels; // indexes into dwelling->creatures that can be recruited from
		ui32 available; // creatures that can be recruited right now, across all levels
	};

	/// list order: by creature level, then by name
	static bool compareEntries(const Entry & a, const Entry & b);

	class CItem : public CIntObject
	{
		RemoteDwellingsWindow * parent;
		const CGDwelling * dwelling;
		bool enabled;

		std::shared_ptr<TransparentFilledRectangle> background;
		std::shared_ptr<CLabel> name;
		std::vector<std::shared_ptr<CAnimImage>> icons;
		std::vector<std::shared_ptr<CLabel>> amounts;

	public:
		CItem(RemoteDwellingsWindow * parent, const Entry & entry);
		void clickPressed(const Point & cursorPosition) override;
	};

	const CGTownInstance * town;
	std::vector<Entry> entries;

	std::shared_ptr<FilledTexturePlayerColored> background;
	std::shared_ptr<CLabel> title;
	std::shared_ptr<CLabel> emptyListLabel;
	std::shared_ptr<CListBox> list;
	std::shared_ptr<CButton> closeButton;

	/// returns indexes of dwelling levels that can be recruited from into this town, see CGDwelling::canRecruitRemotely
	std::vector<size_t> getRecruitableLevels(const CGDwelling * dwelling) const;
	void updateEntries();
	std::shared_ptr<CIntObject> createItem(size_t index);
	void openRecruitment(const CGDwelling * dwelling);

public:
	explicit RemoteDwellingsWindow(const CGTownInstance * town);
};
