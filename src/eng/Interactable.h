#pragma once
#include "eng/Component.h"
#include <string>

namespace eng {

// Something the player can use with the interact key. InteractionScript picks the best candidate in view.
class Interactable : public Component {
public:
    virtual std::string Prompt() const { return prompt; }
    virtual bool Available() const { return true; }
    virtual void Interact(GameObject& who) = 0;

    std::string prompt;
    float range = 3.0f;
};

} // namespace eng
