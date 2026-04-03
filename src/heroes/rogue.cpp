#include "heroes/rogue.hpp"

Rogue::Rogue()
	: Hero(1, 20, 15, 30, 20)
{
}

Hero::ClassType Rogue::GetClassType() const
{
	return ClassType::Rogue;
}

const char* Rogue::GetClassName() const
{
	return "Rogue";
}
