#pragma once

class Hero {
public:
	enum class ClassType {
		Warrior,
		Rogue,
		Sorcerer
	};

	virtual ~Hero() = default;

	virtual ClassType GetClassType() const = 0;
	virtual const char* GetClassName() const = 0;

	int GetLevel() const;
	int GetStrength() const;
	int GetMagic() const;
	int GetDexterity() const;
	int GetVitality() const;

protected:
	Hero(int level, int strength, int magic, int dexterity, int vitality);

private:
	int level_;
	int strength_;
	int magic_;
	int dexterity_;
	int vitality_;
};
