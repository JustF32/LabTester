# Тестовые файлы для лабораторной работы 4

Эти файлы можно добавлять в приложение как работы студента для проверки алгоритмов на строках.

- `student_one_star.cpp` - базовые случаи закрыты, но Rabin-Karp не проверяет коллизии, одиночные алгоритмы реализованы упрощенно, Aho-Corasick заменен последовательным поиском.
- `student_two_stars.cpp` - корректные Rabin-Karp, KMP и Boyer-Moore, но Aho-Corasick реализован наивным поиском каждого шаблона.
- `student_three_stars.cpp` - полноценные реализации, включая trie с failure-ссылками для Aho-Corasick.

Группы тестов: `RabinKarp`, `KMP`, `BoyerMoore`, `AhoCorasick`. Внутри каждой группы есть уровни `Basic`, `Advanced` и `Performance`.
