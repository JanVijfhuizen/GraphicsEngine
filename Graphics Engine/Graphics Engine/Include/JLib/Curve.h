#pragma once

namespace je
{
	// Tool to generate cubic bezier curves with:
	// https://cubic-bezier.com

	// Cubic Bezier Curve, to be exact.
	struct Curve final
	{
		glm::vec2 points[2]{};

		// Evaluates the curve at the target point between 0 and 1.
		[[nodiscard]] float REvaluate(float lerp) const;
		[[nodiscard]] float Evaluate(float lerp) const;
	};

	// Evaluates from two seperate curves, one for the climb and one for the drop.
	[[nodiscard]] float DoubleCurveEvaluate(float lerp, const Curve& up, const Curve& down);

	// Creates a curve that moves from 0 to 1 vertically, and slightly overshoots right before the end.
	[[nodiscard]] Curve CreateCurveOvershooting();
	// Creates a curve that quickly moves to the endpoint, and slows down at the end.
	[[nodiscard]] Curve CreateCurveDecelerate();
}
