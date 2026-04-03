#include "heroes/hero.hpp"

Hero::Hero(int level, int strength, int magic, int dexterity, int vitality)
	: level_(level)
	, strength_(strength)
	, magic_(magic)
	, dexterity_(dexterity)
	, vitality_(vitality)
{
}

int Hero::GetLevel() const
{
	return level_;
}

int Hero::GetStrength() const
{
	return strength_;
}

int Hero::GetMagic() const
{
	return magic_;
}

int Hero::GetDexterity() const
{
	return dexterity_;
}

int Hero::GetVitality() const
{
	return vitality_;
}
