#pragma once

#include "heroes/hero.hpp"

class Rogue final : public Hero {
public:
	Rogue();

	ClassType GetClassType() const override;
	const char* GetClassName() const override;
};
