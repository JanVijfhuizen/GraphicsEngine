#include "pch.h"
#include "JLib/Curve.h"
#include "JLib/Math.h"

namespace je
{
	float Curve::Evaluate(float lerp) const
	{
		lerp = jv::Clamp<float>(lerp, 0, 1);

		glm::vec2 points4[4]{};
		points4[1] = points[0];
		points4[2] = points[0];
		points4[3] = glm::vec2(1);

		const float y = powf(1 - lerp, 3) * points4[0].y + 3 * lerp * powf(1 - lerp, 2) *
			points4[1].y + 3 * powf(lerp, 2) * (1 - lerp) * points4[2].y
			+ powf(lerp, 3) * points4[3].y;

		return y;
	}

	float DoubleCurveEvaluate(const float lerp, const Curve& up, const Curve& down)
	{
		return lerp > .5f ? 1.f - down.Evaluate((lerp - .5f) * 2) : up.Evaluate(lerp * 2);
	}

	Curve CreateCurveOvershooting()
	{
		return
		{
			glm::vec2(.41f, .17f),
			glm::vec2(.34f, 1.37f)
		};
	}

	Curve CreateCurveDecelerate()
	{
		return
		{
			glm::vec2(.23f, 1.05f),
			glm::vec2(1)
		};
	}
}
