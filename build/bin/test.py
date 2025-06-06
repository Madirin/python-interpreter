class A:
    x = 1
class B(A):
    y = 0
b = B()
print(b.y + b.x)