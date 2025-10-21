"""Utility maths helpers used by the image processing pipeline."""

from __future__ import annotations

import math
from typing import Optional, Tuple


Point = Tuple[int, int]


def find_midpoint(point1: Point, point2: Point) -> Point:
    """Return the midpoint between two points."""

    return (int((point1[0] + point2[0]) / 2), int((point1[1] + point2[1]) / 2))


def line_slope(point1: Point, point2: Point) -> float:
    """Calculate the slope of the line passing through two points."""

    if point1[0] == point2[0]:
        return float("inf")
    return (point2[1] - point1[1]) / (point2[0] - point1[0])


def perpendicular_line_slope(slope: float) -> float:
    """Return the slope of the line perpendicular to the supplied slope."""

    if slope == 0:
        return float("inf")
    if slope == float("inf"):
        return 0
    return -1 / slope


def line_equation_point(slope: float, intercept: float, x: float) -> float:
    """Calculate the Y value for a line defined by slope and intercept."""

    return slope * x + intercept


def intersection_point(
    line1_slope: float,
    line1_intercept: float,
    line2_slope: float,
    line2_intercept: float,
) -> Optional[Point]:
    """Calculate the intersection between two infinite lines."""

    if line1_slope == line2_slope:
        return None

    x = (line2_intercept - line1_intercept) / (line1_slope - line2_slope)
    y = line1_slope * x + line1_intercept
    return int(x), int(y)


def find_intersection(A: Point, B: Point, C: Point, D: Point) -> Optional[Point]:
    """Calculate the intersection point of the perpendicular from AB to CD."""

    mid_AB = find_midpoint(A, B)
    slope_AB = line_slope(A, B)
    slope_perpendicular = perpendicular_line_slope(slope_AB)
    c_perpendicular = mid_AB[1] - slope_perpendicular * mid_AB[0]

    slope_CD = line_slope(C, D)
    c_CD = C[1] - slope_CD * C[0]

    return intersection_point(slope_perpendicular, c_perpendicular, slope_CD, c_CD)


def distance_between_points(point1: Point, point2: Point) -> float:
    """Return the Euclidean distance between two points."""

    x1, y1 = point1
    x2, y2 = point2
    return math.hypot(x2 - x1, y2 - y1)
