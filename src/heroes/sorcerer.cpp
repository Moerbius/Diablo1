#include "heroes/sorcerer.hpp"

Sorcerer::Sorcerer()
	: Hero(1, 15, 35, 15, 20)
{
}

Hero::ClassType Sorcerer::GetClassType() const
{
	return ClassType::Sorcerer;
}

const char* Sorcerer::GetClassName() const
{
	return "Sorcerer";
}
