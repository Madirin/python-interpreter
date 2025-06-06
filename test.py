class Point:
    def __init__(self, x, y):
        self.x = x
        self.y = y
class Line(Point):
    def __init__(self, x, y, z):
        Point.__init__(self, x, y)
        self.z = z
p = Point(1, 2)
l = Line(1, 2, 3)
print(l.y)