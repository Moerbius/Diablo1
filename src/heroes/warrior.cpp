#include "heroes/warrior.hpp"

Warrior::Warrior()
	: Hero(1, 30, 10, 20, 25)
{
}

Hero::ClassType Warrior::GetClassType() const
{
	return ClassType::Warrior;
}

const char* Warrior::GetClassName() const
{
	return "Warrior";
}
