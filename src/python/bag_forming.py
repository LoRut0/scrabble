alphabet = [ 'А', 'Б', 'В', 'Г', 'Д', 'Е', 'Ж', 'З', 'И', 'Й', 'К', 'Л', 'М', 'Н', 'О', 'П', 'Р', 'С', 'Т', 'У', 'Ф', 'Х', 'Ц', 'Ч', 'Ш', 'Щ', 'Ъ', 'Ы', 'Ь', 'Э', 'Ю', 'Я', '*' ]
alphabet_price = [ 1, 3, 2, 3, 2, 1, 5, 5, 1, 2, 2, 2, 2, 1, 1, 2, 2, 2, 2, 3, 10, 5, 10, 5, 10, 10, 10, 5, 5, 10, 10, 3, 0 ]
alphabet_freq = [ 10, 3, 5, 3, 5, 9, 2, 2, 8, 4, 6, 4, 5, 8, 10, 6, 6, 6, 5, 3, 1, 2, 1, 2, 1, 1, 1, 2, 2, 1, 1, 1, 3 ]

print(len(alphabet), len(alphabet_price), len(alphabet_freq))
print("price")
for i in range(len(alphabet)) :
    print(f"{alphabet[i]}-{alphabet_price[i]}", end=' ')
print()
print("frequency")
for i in range(len(alphabet)) :
    print(f"{alphabet[i]}-{alphabet_freq[i]}", end=' ')
print()

for i in range(len(alphabet)):
    for j in range(alphabet_freq[i]):
        print(f"{{U'{alphabet[i]}', {alphabet_price[i]}}}, ", end="")

