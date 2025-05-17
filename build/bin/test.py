x = 1
y = 1

def sum(x, y):
    if (x < y):
        for i in range(x+y):
            x = x + y + i
        
        return x

print(sum(x = 5, y = 5))