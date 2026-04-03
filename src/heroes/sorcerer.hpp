#pragma once

#include "heroes/hero.hpp"

class Sorcerer final : public Hero {
public:
	Sorcerer();

	ClassType GetClassType() const override;
	const char* GetClassName() const override;
};
