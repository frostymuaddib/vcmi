/*
 * loseconditions.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#pragma once

#include "abstractsettings.h"

namespace Ui {
class LoseConditions;
}

class LoseConditions : public AbstractSettings
{
	Q_OBJECT

public:
	explicit LoseConditions(QWidget *parent = nullptr);
	~LoseConditions();

	void initialize(MapController & map) override;
	void update() override;
	
public slots:
	void onObjectSelect();
	void onObjectPicked(const CGObjectInstance *);

private slots:
	void on_addConditionButton_clicked();
	void on_editConditionButton_clicked();
	void on_removeConditionButton_clicked();

private:
	Ui::LoseConditions * ui;

	int editingIndex = -1;

	void openConditionDialog();
	void updateStandardDefeatCheck();

	enum class LoseConditionType
	{
		LOSE_CASTLE = 0,
		LOSE_HERO = 1,
		TIME_EXPIRED = 2,
		DAYS_WITHOUT_TOWN = 3
	};

	static bool requiresObjectSelection(int type);

};

