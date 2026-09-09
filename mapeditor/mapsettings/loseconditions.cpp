/*
 * loseconditions.cpp, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */
#include "StdInc.h"
#include "loseconditions.h"
#include "ui_loseconditions.h"
#include "../mapcontroller.h"
#include "../../lib/texts/CGeneralTextHandler.h"
#include "../translator.h"

LoseConditions::LoseConditions(QWidget *parent) :
	AbstractSettings(parent),
	ui(new Ui::LoseConditions)
{
	ui->setupUi(this);
}

LoseConditions::~LoseConditions()
{
	delete ui;
}

static QString posLabel(const CMap & map, int objectIndex)
{
	const auto & pos = map.objects[objectIndex]->pos;
	return QString(" [%1, %2, %3]").arg(pos.x).arg(pos.y).arg(pos.z);
}

bool LoseConditions::requiresObjectSelection(int type)
{
    return type == static_cast<int>(LoseConditionType::LOSE_CASTLE) || type == static_cast<int>(LoseConditionType::LOSE_HERO);
}

void LoseConditions::initialize(MapController & c)
{
	AbstractSettings::initialize(c);

	ui->defeatMessageEdit->setText(QString::fromStdString(
		controller->map()->defeatMessage.toString(&Translator::instance())));
	bool hasStandardDefeat = false;

	for(auto & ev : controller->map()->triggeredEvents)
	{
		if(ev.effect.type != EventEffect::DEFEAT)
			continue;
		if(ev.identifier == "standardDefeat")
		{	
			hasStandardDefeat = true;
			continue;
		}

		auto readjson = ev.trigger.toJson(AbstractSettings::conditionToJson);
		auto linearNodes = linearJsonArray(readjson);

		
		for(auto & json : linearNodes)
		{
			switch(json["condition"].Integer())
			{
				case EventCondition::CONTROL: {
					auto objectType = MapObjectID::decode(json["objectType"].String());
					if(objectType == Obj::TOWN)
					{
						int idx = getObjectByPos<const CGTownInstance>(*controller->map(), posFromJson(json["position"]));
						auto * item = new QListWidgetItem(
							tr("Lose castle: ") + QString::fromStdString(getTownName(*controller->map(), idx)) + posLabel(*controller->map(), idx));
						item->setData(Qt::UserRole, QVariantList{0, idx, 0});
						ui->conditionsList->addItem(item);
					}
					if(objectType == Obj::HERO || objectType == Obj::HERO_PLACEHOLDER)
					{
						int idx = getHeroTargetObjectByPos(*controller->map(), posFromJson(json["position"]));
						auto * item = new QListWidgetItem(
							tr("Lose hero: ") + QString::fromStdString(getHeroName(*controller->map(), idx))  + posLabel(*controller->map(), idx));
						item->setData(Qt::UserRole, QVariantList{1, idx, 0});
						ui->conditionsList->addItem(item);
					}
					break;
				}
				case EventCondition::DAYS_PASSED: {
					int value = json["value"].Integer();
					auto * item = new QListWidgetItem(
						tr("Time expired: ") + expiredDate(value));
					item->setData(Qt::UserRole, QVariantList{2, -1, value});
					ui->conditionsList->addItem(item);
					break;
				}
				case EventCondition::DAYS_WITHOUT_TOWN: {
					int value = json["value"].Integer();
					auto * item = new QListWidgetItem(
						tr("Days without town: ") + QString::number(value));
					item->setData(Qt::UserRole, QVariantList{3, -1, value});
					ui->conditionsList->addItem(item);
					break;
				}
			}
		}
	}

	ui->standardDefeatCheck->setChecked(hasStandardDefeat);
	updateStandardDefeatCheck();
}

void LoseConditions::update()
{
	// triggeredEvents was already cleared by MapSettings before calling update()

	// Standard defeat (7 days without town) is always present
	EventCondition defeatCondition(EventCondition::DAYS_WITHOUT_TOWN);
	defeatCondition.value = 7;

	TriggeredEvent standardDefeat;
	standardDefeat.effect.type = EventEffect::DEFEAT;
	standardDefeat.effect.toOtherMessage.appendTextID("core.genrltxt.8");
	standardDefeat.identifier = "standardDefeat";
	standardDefeat.description.clear();
	standardDefeat.onFulfill.appendTextID("core.genrltxt.7");
	standardDefeat.trigger = EventExpression(defeatCondition);
	if(ui->standardDefeatCheck->isChecked())
		controller->map()->triggeredEvents.push_back(standardDefeat);

	bool customMessage = true;

	for(int i = 0; i < ui->conditionsList->count(); ++i)
	{
		auto * item = ui->conditionsList->item(i);
		auto data = item->data(Qt::UserRole).toList();
		int type = data[0].toInt();
		int objectIndex = data[1].toInt();
		int value = data[2].toInt();

		TriggeredEvent specialDefeat;
		specialDefeat.effect.type = EventEffect::DEFEAT;
		// Each condition gets a unique identifier so multiple can coexist in the map file
		specialDefeat.identifier = "specialDefeat" + std::to_string(i);
		specialDefeat.description.clear();

		// All defeat conditions require the player to be human
		EventExpression::OperatorAll allOf;
		EventCondition isHuman(EventCondition::IS_HUMAN);
		isHuman.value = 1;
		allOf.expressions.push_back(isHuman);

		switch(type)
		{
			case static_cast<int>(LoseConditionType::LOSE_CASTLE): {
				EventExpression::OperatorNone noneOf;
				EventCondition cond(EventCondition::CONTROL);
				cond.objectType = Obj(Obj::TOWN);
				cond.position = controller->map()->objects[objectIndex]->pos;
				noneOf.expressions.push_back(cond);
				allOf.expressions.push_back(noneOf);
				specialDefeat.onFulfill.appendTextID("core.genrltxt.251");
				controller->map()->defeatIconIndex = 1;
				controller->map()->defeatMessage = MetaString::createFromTextID("core.lcdesc.1");
				customMessage = false;
				break;
			}
			case static_cast<int>(LoseConditionType::LOSE_HERO): {
				EventExpression::OperatorNone noneOf;
				EventCondition cond(EventCondition::CONTROL);
				cond.objectType = Obj(Obj::HERO);
				cond.position = controller->map()->objects[objectIndex]->pos;
				noneOf.expressions.push_back(cond);
				allOf.expressions.push_back(noneOf);
				specialDefeat.onFulfill.appendTextID("core.genrltxt.253");
				controller->map()->defeatIconIndex = 2;
				controller->map()->defeatMessage = MetaString::createFromTextID("core.lcdesc.2");
				customMessage = false;
				break;
			}
			case static_cast<int>(LoseConditionType::TIME_EXPIRED): { // Time expired
				EventCondition cond(EventCondition::DAYS_PASSED);
				cond.value = value;
				allOf.expressions.push_back(cond);
				specialDefeat.onFulfill.appendTextID("core.genrltxt.254");
				controller->map()->defeatIconIndex = 3;
				controller->map()->defeatMessage = MetaString::createFromTextID("core.lcdesc.3");
				customMessage = false;
				break;
			}
			case static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN): { // Days without town (custom value)
				EventCondition cond(EventCondition::DAYS_WITHOUT_TOWN);
				cond.value = value;
				allOf.expressions.push_back(cond);
				specialDefeat.onFulfill.appendTextID("core.genrltxt.7");
				break;
			}
		}

		specialDefeat.trigger = EventExpression(allOf);
		controller->map()->triggeredEvents.push_back(specialDefeat);
	}

	if(customMessage)
	{
		controller->map()->defeatMessage = MetaString::createFromTextID(
			mapRegisterLocalizedString("map", *controller->map(),
				TextIdentifier("header", "defeatMessage"),
				ui->defeatMessageEdit->text().toStdString()));
	}
}

void LoseConditions::openConditionDialog()
{
	QDialog dialog(this);
	dialog.setWindowTitle(editingIndex == -1 ? tr("Add condition") : tr("Edit condition"));

	auto * layout = new QVBoxLayout(&dialog);

	// Condition type selector
	auto * typeCombo = new QComboBox;
	typeCombo->addItem(tr("Lose castle"), static_cast<int>(LoseConditionType::LOSE_CASTLE));
	typeCombo->addItem(tr("Lose hero"), static_cast<int>(LoseConditionType::LOSE_HERO));
	typeCombo->addItem(tr("Time expired"), static_cast<int>(LoseConditionType::TIME_EXPIRED));
	typeCombo->addItem(tr("Days without town"), static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN));
	layout->addWidget(typeCombo);

	// Object selector (shown for castle and hero types)
	auto * objectCombo = new QComboBox;
	layout->addWidget(objectCombo);

	// Value input (shown for time/days types)
	auto * valueEdit = new QLineEdit;
	layout->addWidget(valueEdit);

	// Fills objectCombo or valueEdit based on selected type
	auto updateParams = [&](int type)
	{
		objectCombo->clear();
		objectCombo->setVisible(requiresObjectSelection(type));
		valueEdit->setVisible(!requiresObjectSelection(type));

		if(type == static_cast<int>(LoseConditionType::LOSE_CASTLE))
		{
			for(int i : getObjectIndexes<const CGTownInstance>(*controller->map()))
				objectCombo->addItem(QString::fromStdString(getTownName(*controller->map(), i)) + posLabel(*controller->map(), i), i);
		}
		else if(type == static_cast<int>(LoseConditionType::LOSE_HERO))
		{
			for(int i : getHeroTargetObjectIndexes(*controller->map()))
				objectCombo->addItem(QString::fromStdString(getHeroName(*controller->map(), i)) + posLabel(*controller->map(), i), i);
		}
		else if(type == static_cast<int>(LoseConditionType::TIME_EXPIRED))
		{
			valueEdit->setText("2m 1w 1d");
		}
		else if(type == static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN))
		{
			valueEdit->setText("7");
		}
	};

	// Pre-fill when editing an existing condition
	if(editingIndex >= 0)
	{
		auto data = ui->conditionsList->item(editingIndex)->data(Qt::UserRole).toList();
		int type = data[0].toInt();
		typeCombo->setCurrentIndex(type);
		updateParams(type);
		if(requiresObjectSelection(type))
			objectCombo->setCurrentIndex(objectCombo->findData(data[1].toInt()));
		else if(type == static_cast<int>(LoseConditionType::TIME_EXPIRED))
			valueEdit->setText(expiredDate(data[2].toInt()));
		else
			valueEdit->setText(QString::number(data[2].toInt()));
	}
	else
	{
		updateParams(0);
	}

	// Re-populate params whenever the type changes
	connect(typeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
		[&](int idx) { updateParams(idx); });

	auto * buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
	connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
	layout->addWidget(buttons);

	if(dialog.exec() != QDialog::Accepted)
		return;

	// Read the result from the dialog widgets
	int type = typeCombo->currentData().toInt();
	int objectIndex = requiresObjectSelection(type) ? objectCombo->currentData().toInt() : -1;
	int value = (type == static_cast<int>(LoseConditionType::TIME_EXPIRED)) ? expiredDate(valueEdit->text())
	          : (type == static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN)) ? valueEdit->text().toInt()
	          : 0;

	// Build the display string shown in the list
	QString label;
	switch(type)
	{
		case static_cast<int>(LoseConditionType::LOSE_CASTLE): 
			label = tr("Lose castle: ") + QString::fromStdString(getTownName(*controller->map(), objectIndex))  + posLabel(*controller->map(), objectIndex); 
			break;
		case static_cast<int>(LoseConditionType::LOSE_HERO): 
			label = tr("Lose hero: ") + QString::fromStdString(getHeroName(*controller->map(), objectIndex)) + posLabel(*controller->map(), objectIndex); 
			break;
		case static_cast<int>(LoseConditionType::TIME_EXPIRED): 
			label = tr("Time expired: ") + expiredDate(value); 
			break;
		case static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN): 
			label = tr("Days without town: ") + QString::number(value); 
			break;
	}

	QVariantList itemData = {type, objectIndex, value};

	if(editingIndex == -1)
	{
		// Adding a new condition
		auto * item = new QListWidgetItem(label);
		item->setData(Qt::UserRole, itemData);
		ui->conditionsList->addItem(item);
	}
	else
	{
		// Updating an existing condition in place
		auto * item = ui->conditionsList->item(editingIndex);
		item->setText(label);
		item->setData(Qt::UserRole, itemData);
	}
}

void LoseConditions::updateStandardDefeatCheck()
{
	for( int i = 0; i < ui->conditionsList->count(); i++)
	{
		auto data = ui->conditionsList->item(i)->data(Qt::UserRole).toList();
		if(data[0].toInt() == static_cast<int>(LoseConditionType::DAYS_WITHOUT_TOWN))
		{
			ui->standardDefeatCheck->setChecked(false);
			ui->standardDefeatCheck->setEnabled(false);
			return;
		}
	}
	ui->standardDefeatCheck->setEnabled(true);
}

void LoseConditions::on_addConditionButton_clicked()
{
	editingIndex = -1;
	openConditionDialog();
	updateStandardDefeatCheck();
}

void LoseConditions::on_editConditionButton_clicked()
{
	editingIndex = ui->conditionsList->currentRow();
	if(editingIndex >= 0)
		openConditionDialog();
	updateStandardDefeatCheck();
}

void LoseConditions::on_removeConditionButton_clicked()
{
	int row = ui->conditionsList->currentRow();
	if(row >= 0)
		delete ui->conditionsList->takeItem(row);
	updateStandardDefeatCheck();
}

void LoseConditions::onObjectSelect()
{
	// Map picking not yet supported in dialog mode
}

void LoseConditions::onObjectPicked(const CGObjectInstance *)
{
	// Map picking not yet supported in dialog mode
}
