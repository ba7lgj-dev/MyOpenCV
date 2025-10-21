import math


def find_midpoint(point1, point2):
    """计算两点的中点"""
    return (int((point1[0] + point2[0]) / 2), int((point1[1] + point2[1]) / 2))


def line_slope(point1, point2):
    """计算两点间直线的斜率"""
    if point1[0] == point2[0]:  # 避免除以零情况，此时为垂直线
        return float('inf')
    return (point2[1] - point1[1]) / (point2[0] - point1[0])


def perpendicular_line_slope(slope):
    """计算垂直线的斜率"""
    return -1 / slope if slope != float('inf') else 0


def line_equation_point(slope, intercept, x):
    """给定斜率和截距，计算y值"""
    return slope * x + intercept


def intersection_point(line1_slope, line1_intercept, line2_slope, line2_intercept):
    """计算两直线的交点"""
    if line1_slope != line2_slope:  # 确保不是同一直线
        x = (line2_intercept - line1_intercept) / (line1_slope - line2_slope)
        y = line1_slope * x + line1_intercept
        return (int (x),int (y))
    else:
        # 如果是同一直线或者垂直线情况未处理，这里简化处理，实际情况可能需要更复杂的逻辑
        return None


def find_intersection(A, B, C, D):
    """计算垂线与另一线段的交点"""
    # 计算AB中点
    mid_AB = find_midpoint(A, B)

    # 计算AB线段的斜率
    slope_AB = line_slope(A, B)

    # 计算垂直于AB的直线斜率
    slope_perpendicular = perpendicular_line_slope(slope_AB)

    # 由于垂线通过AB中点，可以设定为y = mx + c形式，其中m是斜率，c是y轴截距
    # 用中点解出c
    c_perpendicular = mid_AB[1] - slope_perpendicular * mid_AB[0]

    # 计算线段CD的斜率和截距
    slope_CD = line_slope(C, D)
    c_CD = C[1] - slope_CD * C[0]

    # 计算交点
    intersect_point = intersection_point(slope_perpendicular, c_perpendicular, slope_CD, c_CD)

    return intersect_point

def distance_between_points(point1, point2):
    """计算并返回两点之间的距离"""
    x1, y1 = point1
    x2, y2 = point2
    distance = math.sqrt((x2 - x1)**2 + (y2 - y1)**2)
    return distance