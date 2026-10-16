#pragma once

#include "eng/GameObject.h"

namespace ai
{
    enum class NodeStatus
    {
        Success,
        Failure,
        Running
    };

    class Node
    {
    public:
        virtual ~Node() = default;

        virtual NodeStatus Execute() = 0;
    };

}