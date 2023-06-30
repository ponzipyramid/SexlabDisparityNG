#include "Papyrus.h"
#include <functional>
#include <algorithm>

using namespace RE;
using namespace RE::BSScript;
using namespace REL;
using namespace SKSE;
namespace {
    constexpr std::string_view PapyrusClass = "ArrayMath";

    float MyTest(StaticFunctionTag*) { 
        return 3.3;
    }

    int32_t TestIntFn(StaticFunctionTag*, int32_t value) { 
        return value + 11;
    }

    float TestArrayFn(StaticFunctionTag*, std::vector<float> source) { 
        float total = 0.0f;
        
        for (int i = 0; i < source.size(); i++) total += source[i];

        return total;
    }

    std::vector<float> Copy(StaticFunctionTag*, std::vector<float> source) {
        std::vector<float> copy(source.size());

        for (int i = 0; i < source.size(); i++) copy[i] = source[i];

        return copy;
    }

    template <class Func>
    std::vector<float> ArrayOp(std::vector<float> lhs, std::vector<float> rhs, Func func) {
        std::vector<float> result(lhs.size());

        for (int i = 0; i < lhs.size(); i++) result[i] = func(lhs[i], rhs[i]);

        return result;
    }

    template <class Func>
    std::vector<float> ArrayScalarOp(std::vector<float> lhs, float rhs, Func func) {
        std::vector<float> result(lhs.size());

        for (int i = 0; i < lhs.size(); i++) result[i] = func(lhs[i], rhs);

        return result;
    }

    std::vector<float> Add(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) { 
        return ArrayOp(lhs, rhs, std::plus<float>());
    }

    std::vector<float> Sub(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) {
        return ArrayOp(lhs, rhs, std::minus<float>());
    }

    std::vector<float> Mul(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) {
        return ArrayOp(lhs, rhs, std::multiplies<float>());
    }

    std::vector<float> Div(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) { 
        return ArrayOp(lhs, rhs, std::divides<float>());
    }

    std::vector<float> AddScalar(StaticFunctionTag*, std::vector<float> lhs, float scalar) {
        return ArrayScalarOp(lhs, scalar, std::plus<float>());
    }

    std::vector<float> MulScalar(StaticFunctionTag*, std::vector<float> lhs, float scalar) {
        return ArrayScalarOp(lhs, scalar, std::multiplies<float>());
    }

    std::vector<float> Clamp(StaticFunctionTag*, std::vector<float> source, float minValue, float maxValue) {
        std::vector<float> result(source.size());

        for (int i = 0; i < source.size(); i++) result[i] = std::clamp(source[i], minValue, maxValue);

        return result;
    }

    float MinOperation(float lhs, float rhs) { return std::min(lhs, rhs); }
    float MaxOperation(float lhs, float rhs) { return std::min(lhs, rhs); }
    
    std::vector<float> Min(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) { 
        return ArrayOp(lhs, rhs, MinOperation);
    }

    std::vector<float> Max(StaticFunctionTag*, std::vector<float> lhs, std::vector<float> rhs) {
        return ArrayOp(lhs, rhs, MaxOperation); 
    }

    std::vector<float> MinScalar(StaticFunctionTag*, std::vector<float> lhs, float rhs) {
        return ArrayScalarOp(lhs, rhs, MinOperation);
    }

    std::vector<float> MaxScalar(StaticFunctionTag*, std::vector<float> lhs, float scalar) {
        return ArrayScalarOp(lhs, scalar, MaxOperation);
    }

    float InterpolateScalar(StaticFunctionTag*, float from, float to, float input) {
        if (to == from) {
            return input < from ? 0.0 : 1.0;
        }

        float p = (input - from) / (to - from);

        return std::min(1.0f, std::max(0.0f, p));
    }

    const int ixDebuffMax = 0;
    const int ixDebuffFrom = 2;
    const int ixDebuffTo = 4;
    const int ixBuffMax = 1;
    const int ixBuffFrom = 3;
    const int ixBuffTo = 5;
    const int ixStart = 10;
    const int ixEnd = 89;
    const int modifiersLength = 90;
    const int accumulateLength = (modifiersLength - ixStart) / 2;
    const int extractLimit = 128;
    std::vector<float> CalculateModifiers(StaticFunctionTag*, std::vector<float> sourceModifiers, float value,
                                          std::string debugTag) {
        SKSE::log::info("CalculateModifiers");

        std::vector<float> modifierValues;
        modifierValues.resize(modifiersLength);

        if (sourceModifiers.size() < modifiersLength) {
            return modifierValues;
        }

        float debuffScale = sourceModifiers[ixDebuffMax] * 0.01;
        float debuffFrom = sourceModifiers[ixDebuffFrom];
        float debuffTo = sourceModifiers[ixDebuffTo];
        float interpolatedDebuff = InterpolateScalar(NULL, debuffFrom, debuffTo, value);

        float effectiveDebuff = interpolatedDebuff * debuffScale;

        float buffScale = sourceModifiers[ixBuffMax] * 0.01;
        float buffFrom = sourceModifiers[ixBuffFrom];
        float buffTo = sourceModifiers[ixBuffTo];
        float interpolatedBuff = InterpolateScalar(NULL, buffFrom, buffTo, value);

        float effectiveBuff = interpolatedBuff * buffScale;

        for (int ii = ixStart; ii < ixEnd; ii += 2) {
            float debuff = sourceModifiers[ii] * effectiveDebuff;
            float buff = sourceModifiers[ii + 1] * effectiveBuff;
            modifierValues[ii] = debuff;
            modifierValues[ii + 1] = buff;
        }

        return modifierValues;
    }

    std::vector<float> AccumulateModifiers(StaticFunctionTag*, std::vector<float> accumulateTarget, float masterDebuff,
                                           float masterBuff, std::vector<float> sourceModifiers) {

        if (sourceModifiers.size() < modifiersLength || accumulateTarget.size() < accumulateLength) {
            return accumulateTarget;
        }

        int mm = 0;
        for (int jj = ixStart; jj < ixEnd; ++mm, jj += 2) {
            float value = 0.01 * (masterDebuff * sourceModifiers[jj] + masterBuff * sourceModifiers[jj + 1]);
            accumulateTarget[mm] += value;
        }

        return accumulateTarget;
    }
}

bool ArrayMath::RegisterFunctions(IVirtualMachine* vm) {
    vm->RegisterFunction("MyTest", PapyrusClass, MyTest);
    vm->RegisterFunction("TestIntFn", PapyrusClass, TestIntFn);
    vm->RegisterFunction("TestArrayFn", PapyrusClass, TestArrayFn);
    vm->RegisterFunction("Copy", PapyrusClass, Copy);
    vm->RegisterFunction("Add", PapyrusClass, Add);
    vm->RegisterFunction("Sub", PapyrusClass, Sub);
    vm->RegisterFunction("Mul", PapyrusClass, Mul);
    vm->RegisterFunction("Div", PapyrusClass, Div);
    vm->RegisterFunction("AddScalar", PapyrusClass, AddScalar);
    vm->RegisterFunction("MulScalar", PapyrusClass, MulScalar);
    vm->RegisterFunction("Clamp", PapyrusClass, Clamp);
    vm->RegisterFunction("Min", PapyrusClass, Min);
    vm->RegisterFunction("Max", PapyrusClass, Max);
    vm->RegisterFunction("MinScalar", PapyrusClass, MinScalar);
    vm->RegisterFunction("MaxScalar", PapyrusClass, MaxScalar);
    vm->RegisterFunction("InterpolateScalar", PapyrusClass, InterpolateScalar);
    vm->RegisterFunction("CalculateModifiers", PapyrusClass, CalculateModifiers);
    vm->RegisterFunction("AccumulateModifiers", PapyrusClass, AccumulateModifiers);

	return true;
}