Ниже я попробую провести тебя «по-дружески» через каждый из ключевых файлов нашего интерпретатора, объясняя, зачем они нужны, какие сущности в них хранятся и как всё вместе работает. Я буду давать максимум «живых» пояснений, отталкиваясь от практики, а не от академической теории. Допустим, ты рассказы­ешь преподавателю «от себя», показываешь, что понимаешь, зачем всё сделано именно так, и как это пригодится в дальнейшем.

---

## 1. object.hpp

### 1.1 Общая идея класса `Object`

1. **Цель файла.**
   `object.hpp` — это сердце системы представления «значений» (объектов) во время выполнения кода. В Python (и в нашем упрощённом варианете) всё в рантайме представлено в виде объектов: целые числа, строки, списки, словари, функции, классы, экземпляры классов и т. д. Класс `Object` и всё, что от него наследуется, ­– это наш общий «базовый тип» для всех этих сущностей.

2. **Почему именно OOP-стиль.**

   * Мы хотим, чтобы у всех наших объектов были виртуальные методы вроде `__add__`, `__getitem__`, `__call__`, `__getattr__` и т. п. От этого выигрывают сразу несколько вещей:

     * **Полиморфизм.** Например, когда в выражении `a + b` мы не знаем, какие именно объекты лежат в `a` и `b` (число, строка, список); каждый их класс реализует метод `__add__` по-своему. Мы просто пишем `leftVal->__add__(rightVal)`, а в рантайме C++ найдёт нужную реализацию.
     * **Удобство роста системы.** Если завтра ты решишь добавить новые «встроенные типы» (например, множества или новые коллекции), ты просто создаёшь новый класс, наследуешь от `Object` и реализуешь или переопределяешь нужные методы.
     * **Ясность кода.** Меньше условных операторов «if (type==int) … else if (type==str) …». Вместо этого — виртуальные вызовы.

3. **Структура `Object`.**

   ```cpp
   class Object {
   public:
       virtual ~Object() = default;

       // Арифметические операции. Базовая реализация кидает ошибку:
       virtual ObjectPtr __add__(ObjectPtr right) { throw RuntimeError("unsupported +"); }
       virtual ObjectPtr __sub__(ObjectPtr right) { throw RuntimeError("unsupported -"); }
       virtual ObjectPtr __mul__(ObjectPtr right) { throw RuntimeError("unsupported *"); }
       virtual ObjectPtr __div__(ObjectPtr right) { throw RuntimeError("unsupported /"); }

       // Доступ по индексу (list[i], dict[key], str[i], …)
       virtual ObjectPtr __getitem__(ObjectPtr idx) { throw RuntimeError("not subscriptable"); }
       virtual void __setitem__(ObjectPtr key, ObjectPtr value) { throw RuntimeError("object does not support item assignment"); }

       // Атрибуты: obj.x = ... и доступ obj.x
       virtual void __setattr__(const std::string &name, ObjectPtr value) { throw RuntimeError("no attribute"); }
       virtual ObjectPtr __getattr__(const std::string &name) { throw RuntimeError("no attribute"); }

       // Проверка «in»: например, "x in some_collection"
       virtual bool __contains__(ObjectPtr item) { throw RuntimeError("object is not iterable"); }

       // Вызов: obj(...) 
       virtual ObjectPtr __call__(const std::vector<ObjectPtr> & args) { throw RuntimeError("object is not callable"); }

       // Два обязательных чисто виртуальных метода:
       virtual std::type_index type() const = 0;
       virtual std::string repr() const = 0;
   };
   ```

   * Все методы, кроме `type()` и `repr()`, по умолчанию кидают `RuntimeError`. То есть если, скажем, ты случайно захочешь сложить строку и число, без поддержки `PyString::__add__`, у тебя упадёт понятная ошибка «unsupported operand types for +».
   * `type() const` возвращает `std::type_index`, чтобы можно было на этапе выполнения определить точный класс объекта (например, сравнить с `typeid(PyList)`, если нужно).
   * `repr()` возвращает строковое представление (вроде `"123"`, `"\"hello\""` или `"[1, 2, 3]"`), которое используется, когда мы, например, печатаем значение через `print()`.

4. **Как мы храним «все значения».**

   * Каждый «значимый» класс (число, строка, список, функция и т. д.) наследует от `Object`. Например:

     ```cpp
     class PyInt : public Object {
         int value;
     public:
         PyInt(int v) : value(v) {}
         ObjectPtr __add__(ObjectPtr right) override { … }
         std::type_index type() const override { return typeid(PyInt); }
         std::string repr() const override { return std::to_string(value); }
         int get() const { return value; }
     };
     ```
   * Мы никогда не храним «голый `int`», мы всегда оборачиваем его в `std::shared_ptr<PyInt>`.
     Объект `ObjectPtr` — это `using ObjectPtr = std::shared_ptr<Object>`. Такая схема двухуровневая:

     1. Семантически — у нас целое значение, мы работаем с ним через методы `__add__`, `__div__` и так далее.
     2. На уровне памяти — каждый конкретный объект живёт в куче, и везде ходим по `shared_ptr`, чтобы не думать о владении и не писать управляющий код по освобождению вручную.

5. **Перечисление базовых классов:**

   * `PyNone` – «тип None». Ничего не делает, просто `repr() == "None"`.
   * `PyInt` – «целые». Хранит `int value;` и реализует `__add__`, `__sub__`, `__mul__`, `__div__`, умеет взаимодействовать с булевыми и float-значениями (делает неявное приведение).
   * `PyBool` – «булево» (хранит `bool`). По сути унаследован от `Object`, но перепрограммирует те же арифметические операции (ведь в Python `True + 2 == 3`, и т. д.).
   * `PyFloat` – «вещественное» (хранит `double`).
   * `PyString` – «строка». Хранит `std::string`. Реализует:

     * `__add__` (конкатенация);
     * `__mul__` (`"abc" * 3` → `"abcabcabc"`);
     * `__getitem__` (по индексу возвращает `PyString` из одного символа);
     * `__contains__` (возвращает `PyBool` при проверке подстроки).
   * `PyList` – «список». Хранит `std::vector<ObjectPtr> elems;`. Реализует:

     * `__add__` (конкатенация двух списков);
     * `__mul__` (повторение списка);
     * `__getitem__` и `__setitem__` (доступ и присвоение по целочисленному индексу);
     * `__contains__` (поиск по `repr()` элементов).
   * `PyDict` – «словарь». Хранит вектор пар `(ключ,значение)`, каждый из которых — `(ObjectPtr,ObjectPtr)`.
     Реализует:

     * `__setitem__` (помещает пару; если ключ (по `repr()`) уже был, перезаписывает);
     * `__getitem__` (достаёт значение по ключу, если нет – кидает `KeyError`);
     * `__contains__` (проверяет наличие ключа).
   * `PySet` – «множество». Хранит `std::vector<ObjectPtr> elems;`, в конструкторе от вектора убирает дубли.
     Реализует:

     * `__add__` как объединение двух множеств;
     * `__contains__` (проверка вхождения по `repr()`).
   * `PyBuiltinFunction` – «встроенная функция».
     Содержит:

     * `std::string name;`
     * `std::function<ObjectPtr(const std::vector<ObjectPtr>&)> fn;`
       При вызове `__call__` просто перенаправляет на `fn(args)`. То есть все наши «билтинговые» (print, range, len, dir, enumerate) мы создаём как объекты `PyBuiltinFunction`, упаковываем в таблицу символов, и когда кто-то пишет `print(…)` или `len(x)` → мы просто вызываем `__call__` у этого объекта.
   * `PyFunction` – «пользовательская функция».

     * Хранит `name`, указатель на AST-узел `FuncDecl *decl` (чтобы знать тело, имена параметров, дефолтные значения),
     * «лексическое окружение» (shared\_ptr<SymbolTable>),
     * список имён обязательных параметров (`posParams`),
     * уже вычисленные default-значения (`defaultValues`).
       При вызове `__call__` делает чуть сложную логику: новый локальный `Executor`, enter\_scope, связывает аргументы → выполняет тело через `accept(exec)`, ловит `ReturnException`, leave\_scope → возвращает результат.
   * `PyInstance` и `PyClass` – объекты, связанные с классами и экземплярами.

     * `PyClass` хранит своё имя, `std::shared_ptr<PyDict> classDict` (атрибуты и методы класса) и указатель на родителя (если наследуется).
     * `PyInstance` хранит `std::shared_ptr<PyClass> classPtr` и свой `instanceDict` (свои атрибуты).

6. **Идея хранения «значений» в ООП-стиле.**

   * Всюду, где в твоей программе нужно «какое-то значение» (результат выражения, возвращаемое значение функции, элемент списка и т. д.), ты работаешь с `ObjectPtr`, то есть с `std::shared_ptr<Object>`.
   * С помощью динамического приведения ( `std::dynamic_pointer_cast<PyInt>(obj)` ) ты проверяешь, что в данном `ObjectPtr` лежит именно `PyInt`.
   * Когда нужно выполнить, например, сложение, не пишешь `if isInt … else if isFloat …`. Ты просто вызываешь `objL->__add__(objR)`. Внутри `__add__` уже «узнаётся» тип `right`: если это `PyInt`, возвращаем новый `PyInt`; если это `PyFloat`, создаём `PyFloat`; если тип неподдерживаемый – кидаем `RuntimeError`.
   * Аналогично доступ по индексу: `base->__getitem__(idx)`.

### 1.2 Перечисление вложенных объявлений и реализаций

В `object.hpp` сначала идут **форвардные объявления** (чтобы можно было внутри класса `Object` держать `shared_ptr<PyFunction>` и т. п., даже если их тела ещё не описаны). Затем:

1. **`class Object`:**
   Общая база, см. выше. Обращаю внимание, что здесь есть только сигнатуры методов (они определены внутри `inline` ниже или в отдельных CPP-файлах).

2. **`class PyNone : public Object`:**

   * Всего две строчки:

     ```cpp
     std::type_index type() const override { return typeid(PyNone); }
     std::string repr() const override { return "None"; }
     ```
   * У `PyNone` нет арифметики или индексирования – если ты попытаешься что-то с ним сделать (`None + 5`), вызов `__add__` унаследуется от `Object` и упадёт с сообщением «unsupported operand types for +».

3. **`class PyInt` (и `PyBool`, `PyFloat`, `PyString`, `PyList`, `PyDict`, `PySet`, `PyBuiltinFunction`, `PyFunction`, `PyInstance`, `PyClass`):**

   * Каждому из них сначала даётся **объявление**: поля, конструктор, сигнатуры методов — без тела. Например,

     ```cpp
     class PyInt : public Object {
         int value;
     public:
         PyInt(int v);
         ObjectPtr __add__(ObjectPtr right) override;
         // ...
         std::type_index type() const override;
         std::string repr() const override;
         int get() const;
     };
     ```
   * После того как объявлены ВСЕ классы, в **одном большом блоке** идёт реализация (все методы `inline`) в том же заголовочном файле. Таким образом гарантируется, что внутри `PyInt::__add__` «уже существует» класс `PyFloat`, `PyBool` и другие, и можно спокойно писать `std::dynamic_pointer_cast<PyFloat>(right)`.

4. **Реализации `PyInt`, `PyBool`, `PyFloat`:**

   * Все арифметические методы (`__add__`, `__sub__`, `__mul__`, `__div__`) пробуют «снизу вверх» два случая:

     1. Попытаться привести `right` к тому же типу (например, если у тебя `PyInt` и `right` тоже `PyInt`, сделать сложение `int`+`int`).
     2. Попытаться привести к «более широкому» типу (у `PyInt` это `PyFloat`).
     3. У булевых (`PyBool`) учтено, что `bool` может вести себя как `0/1`.
   * Если `right` — неподдерживаемый тип, бросается `RuntimeError("unsupported operand types for +: 'int' and '" + right->repr() + "'")`.
   * Например, для `PyInt::__mul__` есть также второй смысл: `int * PyString` → повторение строки (традиционно в Python).

5. **Реализация `PyString`:**

   * `__add__` – конкатенация, если `right` – тоже `PyString`.
   * `__mul__` – может умножать только на `PyInt` или `PyBool`, создавая новую строку в цикле.
   * `__getitem__` – ожидает целочисленный индекс (в `PyInt` или `PyBool`), возвращает `PyString(1 символ)` или падает с «string index out of range» или «indices must be integers».
   * `__contains__` – реализован как поиск подстроки: смотрим `value.find(sub->value)`.

6. **Реализация `PyList`:**

   * Хранит `std::vector<ObjectPtr> elems`.
   * `__add__` (список + список → конкатенация).
   * `__mul__` (повторение списка), аналогично строке.
   * `__getitem__` и `__setitem__` с проверкой индекса.
   * `__contains__` — линейный поиск по `repr()`.
   * `repr()` строит строку `[el1_repr, el2_repr, …]`.

7. **Реализация `PyDict`:**

   * Хранит `std::vector<std::pair<ObjectPtr,ObjectPtr>> items;`.
   * `__setitem__` пробегает vector, если ключ совпадает по `repr()`, перезаписывает, иначе добавляет новую пару.
   * `__getitem__` линейно ищет ключ — либо возвращает значение, либо бросает `RuntimeError("KeyError: " + idx->repr())`.
   * `__contains__` аналогично ищет ключ.
   * `repr()` строит строку вида `"{key1_repr: value1_repr, key2_repr: value2_repr}"`.

8. **Реализация `PySet`:**

   * Схожа с `PyList`, но в конструкторе (и в `__add__`) мы удаляем дубли по `repr()`.
   * `__contains__` ищет по `repr()`.
   * `repr()` строит `{elem1_repr, elem2_repr, …}`.

9. **Реализация `PyBuiltinFunction`:**

   * Конструктор просто запоминает имя и лямбда-функцию `fn`.
   * При вызове `__call__(args)` вызывает `fn(args)`; в случае ошибки в `fn` бросается `RuntimeError`, и мы его пробрасываем дальше.
   * `repr()` возвращает `"<built-in function " + name + ">"`.

10. **Реализация `PyInstance` и `PyClass`:**

    * `PyClass`:

      * Конструктор принимает имя класса и необязательный указатель на родительский класс (чтобы поддержать единственное наследование).
      * `classDict` — это `PyDict`, куда мы при объявлении класса будем класть либо поля, либо методы (они же функции).
      * `__getattr__`:

        1. Смотрит, есть ли имя `attrName` в собственном `classDict`.
        2. Если нет и `parent` != nullptr, рекурсивно спускается вверх по иерархии.
        3. Если не нашли — бросаем `RuntimeError("object has no attribute '...'" ).`
      * `__setattr__`: просто делает `classDict->__setitem__(attrName, value)` — т. е. любые присваивания к атрибутам класса сразу ложатся в `classDict`.
      * `__call__`: когда кто-то пишет `X(...)`, где `X` — `PyClass`, мы:

        1. Создаём `auto instance = std::make_shared<PyInstance>(shared_from_this())`. Здесь именно `shared_from_this()` – это хитрость от `std::enable_shared_from_this` (об этом чуть ниже).
        2. Ищем метод `__init__` в цепочке `__getattr__`. Если нашли, приводим к `PyFunction` и вызываем так, будто точно знаем, что первый аргумент — это self. Если `__init__` упадёт с `ReturnException`, мы игнорируем возвращаемое значение (именно так работает Python — любое значение, которое возвращает `__init__`, просто молча «теряется»).
        3. Если `__getattr__("__init__")` кинул `RuntimeError("object has no attribute '__init__'")`, значит у класса нет инициализатора — просто вернём голый неинициализированный `instance`.
        4. В результате `X(...)` вернёт `PyInstance`.
    * `PyInstance`:

      * Содержит `std::shared_ptr<PyClass> classPtr;` и `std::shared_ptr<PyDict> instanceDict;`.
      * `__getattr__(name)`:

        1. Сначала проверяем, есть ли `name` в `instanceDict->__contains__(key)`. Если да, возвращаем `instanceDict->__getitem__(key)`.
        2. Иначе пробуем в `classPtr->__getattr__(name)`. Если там бросают `RuntimeError`, мы этого не скрываем, а кидаем дальше.
      * `__setattr__(name, value)`: просто `instanceDict->__setitem__(key, value)`.
      * `__call__`: у экземпляра «вне класса» не реализован, сразу кидает `RuntimeError("object is not callable")`.
      * `repr()` строит что-то вроде `"<Instance of " + classPtr->name + " at " + this_pointer + ">"`.

11. **Зачем `public std::enable_shared_from_this<PyClass>`.**

    * Это стандартный приём в C++, чтобы внутри метода `__call__` безопасно получить `shared_ptr<PyClass>` на сам объект.
    * Когда мы создаём объект `auto classObj = std::make_shared<PyClass>(...)`, теперь внутри `classObj` работают «механизмы» `enable_shared_from_this`.

      * В частности, когда мы пишем `shared_from_this()`, мы получаем ещё один `shared_ptr<PyClass>`, который указывает на тот же самый объект.
      * Если бы мы просто хранили `this` (голый указатель), то после выхода из метода возникало бы «висячее» владение: нельзя гарантировать, что где-то не удалят начальный `shared_ptr`, и наш «сыроузер» (голый `this`) превратится в «дикий» указатель.
    * Практически это нужно, чтобы при вызове класса (<tt>X(...)</tt>) создавать экземпляр и правильно «протаскивать» в поле `classPtr` у `PyInstance` именно тот самый `shared_ptr<PyClass>`, который произошёл из контекста, а не отдельный голый указатель.

12. **Остаток файла — полные реализации (inline) всех методов.**

    * Благодаря тому, что в начале файла перечислены «форвард­но» все классы (`class PyInt;`, `class PyFloat;` и т. д.), мы можем сразу внутри `inline PyInt::__add__` без опаски использовать `std::dynamic_pointer_cast<PyFloat>(...)`, зная, что полное описание класса `PyFloat` уже ниже по тексту.
    * Это удобно, потому что весь базовый функционал (типы и их операции) находится в одном файле. В больших проектах, может быть, удобнее разнести по CPP-файлам, но здесь нам удобнее иметь всё «в одном месте».

13. **RuntimeError.**

    * Это простой структурный класс (`struct RuntimeError : std::runtime_error { using std::runtime_error::runtime_error; };`). Мы используем его везде, где хотим «упасть» с нужным текстом.
    * Своего рода аналог `Python`-ошибок времени выполнения (`TypeError`, `ValueError`, `KeyError`, `ZeroDivisionError`), но мы их все представляем единым типом `RuntimeError`; единственное отличие — текст, который мы передаём в конструктор.

---

## 2. scope.hpp

### 2.1 Общая идея «области видимости»

1. **Цель файла.**
   `scope.hpp` описывает класс `Scope`, который инкапсулирует стек таблиц символов при выполнении программы. В Python у нас есть лексические области видимости (локальная в функции, глобальная, вложенные области в классах и т. д.). `Scope` помогает сохранять, где мы сейчас находимся, и выполнять корректный поиск переменных:

   * `insert(Symbol)` – добавляет новую переменную (имя, тип, значение, указатель на AST) именно в текущую (локальную) таблицу (если ещё нет) или перезаписывает.
   * `lookup(name)` – ищет в текущей и всех внешних (родительских) таблицах, вплоть до глобальной.
   * `lookup_local(name)` – ищет только в рамках текущей таблицы (без подъёма наверх).
   * `enter_scope()` – создаёт новую таблицу символов, «вложенную» внутрь текущей.
   * `leave_scope()` – возвращает «наружу», к родительской таблице.

2. **Почему так.**

   * Когда ты заходишь внутрь функции, тебе нужно *отдельное* пространство имён для её параметров и локальных переменных. Но при этом глобальные (или встроенные) символы (print, range, len) должны остаться видны.
   * Похожая идея при обработке блоков кода (в Python блоки сами по себе не создают новых лексических скоупов, но, скажем, класс `Scope` может понадобиться, если ты решишь ввести вложенные функции).
   * В `Scope` один единственный приватный член: `std::shared_ptr<SymbolTable> current`.

     * При `enter_scope()` создаём новую `SymbolTable`, чей родитель – это `current`, и «спускаемся» внутрь.
     * При `leave_scope()` проверяем, есть ли у `current` родитель (`find_parent()` у `SymbolTable`), и «выбираемся» наверх.

3. **Как `SymbolTable` хранит переменные.**

   * Визуально в `symbol_table.hpp` (не показан в условии, но можно догадаться) есть что-то вроде:

     ```cpp
     struct Symbol {
         std::string name;
         SymbolType type;      // enum { Variable, Parameter, Function, BuiltinFunction, UserClass }
         ObjectPtr value;      // текущее значение
         ASTNode *decl;        // указатель на AST узел для удобства диагностики (например, FuncDecl*, FieldDecl*, …)
     };

     class SymbolTable {
         std::unordered_map<std::string, Symbol> map;
         std::shared_ptr<SymbolTable> parent; // parent скоуп
     public:
         SymbolTable(std::shared_ptr<SymbolTable> parent);
         bool insert(const Symbol &sym);      // вставляет, возвращает false, если уже есть
         Symbol *lookup(const std::string &name);       // поднимается вверх по цепочке parent
         Symbol *lookup_local(const std::string &name); // только в этом map
         std::shared_ptr<SymbolTable> find_parent() const { return parent; }
     };
     ```
   * Поэтому `Scope::insert` добавляет символ в текущую таблицу, а `lookup` пробегает по цепочке родителей.

4. **Практический сценарий.**

   * **Выполнение `Executor` (в executer.cpp)** проинится: при старте у нас создаётся `Executor exec;`. Конструктор `Executor` создаёт в нём `scope` (по умолчанию это глобальная таблица) и сразу же в неё вставляет `print`, `range`, `len`, `dir`, `enumerate` как «встроенные функции». То есть в глобальном скоупе сразу есть имена `print`, `range` и т. д.
   * Когда встречается объявление функции `def f(...): …`, мы в глобальном скоупе сохраняем `f: PyFunction`.
   * Когда внутри этого `f` вызываем ещё какую-то функцию, сначала создаём `enter_scope()`, туда добавляем параметры и локальные переменные. При выходе – `leave_scope()`.
   * При обращении к переменной `x` мы ищем сначала в локальном скоупе, если не нашли – идём в родительский, и так далее (именно `lookup(name)`). Если до вершины дошли и всё равно не нашли, кидаем `RuntimeError("name 'x' is not defined")`.

Таким образом `scope.hpp` вместе с `symbol_table.hpp` решает вопрос «где положена эта переменная, как её найти и куда записать, когда есть присваивание».

---

## 3. type\_registry.hpp

### 3.1 Общая идея

1. **Зачем нужен `TypeRegistry`.**
   Мы хотим у некоторых объектов (например, у `PyInt`, `PyString`, `PyList`) иметь метод-таблицы (как в реальном Python: `int` знает, как работает `int.__add__`, `str` знает, как работает `str.__contains__` и т. д.). Но мы также хотим, чтобы когда идёт `obj.x()`, мы не писали тонну `if (typeid(obj)==typeid(PyInt)) { … } else if ( … )…`.
   Вместо этого мы можем хранить **динамическую таблицу методов**, привязанных к типу `std::type_index`.

   * Например, у типа `PyString` мы хотим зарегистрировать метод `__contains__`, который в runtime будет ссылаться на конкретную реализацию `PyString::__contains__`.
   * Тогда в `Executor::visit(CallExpr &node)` мы сможем проверить тип объекта (через `callee->type()`), узнать у `TypeRegistry::getMethod(type, "__contains__")`, а дальше вызвать полученный `std::function<ObjectPtr(ObjectPtr,const vector<ObjectPtr>&)>`.

2. **Что хранится внутри.**

   ```cpp
   class TypeRegistry {
   public:
       using Method = std::function<ObjectPtr(ObjectPtr, const std::vector<ObjectPtr>&)>;

       static TypeRegistry& instance() {
           static TypeRegistry inst;
           return inst;
       }

       void registerType(const std::type_index &ti) {
           // Если ещё нет такой записи, создать пустую map<methodName, Method>
           if (tables.find(ti) == tables.end()) {
               tables[ti] = {};
           }
       }

       void registerMethod(const std::type_index &ti, const std::string &name, Method fn) {
           registerType(ti);
           tables[ti][name] = std::move(fn);
       }

       Method* getMethod(const std::type_index &ti, const std::string &name) {
           auto it = tables.find(ti);
           if (it == tables.end()) return nullptr;
           auto jt = it->second.find(name);
           if (jt == it->second.end()) return nullptr;
           return &jt->second;
       }

       void registerBuiltins() {
           // В  builtinMethodsMap заранее описано, какие методы нужно зарегистрировать для PyInt, PyBool, PyFloat, PyString, PyList, PyDict, PySet
           // Мы просто проходимся по builtinMethodsMap и для каждого типа вызываем registerMethod(...)
       }

   private:
       TypeRegistry() {}
       std::unordered_map<std::type_index,
                          std::unordered_map<std::string, Method>> tables;

       std::unordered_map<std::type_index,
         std::vector<std::pair<std::string, Method>>> builtinMethodsMap = {
           { typeid(PyInt), {
               { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
           }},
           { typeid(PyBool), {
               { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
           }},
           { typeid(PyFloat), {
               { "__add__", [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__sub__", [](ObjectPtr self, auto &args){ return self->__sub__(args[0]); } },
           }},
           { typeid(PyString), {
               { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
               { "__contains__", [](ObjectPtr self, auto &args){
                     bool ok = self->__contains__(args[0]);
                     return std::make_shared<PyBool>(ok);
                 }
               },
           }},
           { typeid(PyList), {
               { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
               { "__contains__", [](ObjectPtr self, auto &args){
                     bool ok = self->__contains__(args[0]);
                     return std::make_shared<PyBool>(ok);
                 }
               },
           }},
           { typeid(PyDict), {
               { "__getitem__",  [](ObjectPtr self, auto &args){ return self->__getitem__(args[0]); } },
               { "__contains__", [](ObjectPtr self, auto &args){
                     bool ok = self->__contains__(args[0]);
                     return std::make_shared<PyBool>(ok);
                 }
               },
           }},
           { typeid(PySet), {
               { "__add__",      [](ObjectPtr self, auto &args){ return self->__add__(args[0]); } },
               { "__contains__", [](ObjectPtr self, auto &args){
                     bool ok = self->__contains__(args[0]);
                     return std::make_shared<PyBool>(ok);
                 }
               },
           }},
       };
   };
   ```

   * `tables` – это мапа «тип → (имя метода → реализация метода)».
   * `builtinMethodsMap` хранит изначальный список, что для каждого базового типа мы заранее хотим зарегистрировать (например, у `PyString` — `__add__`, `__getitem__`, `__contains__`).
   * Метод `registerBuiltins()` вызывается ровно один раз (обычно в конструкторе `Executor`), чтобы заполнить `tables`. Сюда же могла бы заводиться поддержка пользовательских методов (например, при создании классов).
   * **Как это используется в рантайме?**

     1. Когда мы видим выражение `a.b(c)`, по сути это `AttributeExpr`, а потом `CallExpr`. После вычисления `a.b` мы получим из `__getattr__` у `a` какой-то `ObjectPtr fnobj`. Если это не `PyBuiltinFunction` и не `PyFunction`, а, скажем, произвольный объект, то мы допустим, что для него зарегистрирован метод `"__call__"` в `TypeRegistry`.
     2. В `Executor::visit(CallExpr)`, после того как мы получили `callee` и `args`, мы делаем:

        * Попытка привести `callee` к `PyBuiltinFunction`, если получилось — вызываем `PyBuiltinFunction->__call__`.
        * Иначе — попробовать привести к `PyFunction`: если получилось — обрабатывать как пользовательскую функцию.
        * Иначе — последняя попытка: смотрим, есть ли у типа `callee` метод `"__call__"` в `TypeRegistry::instance().getMethod(callee->type(),"__call__")`. Если он там есть, выполняем его.
        * Если ничего из этого не прошло — кидаем «TypeError: object is not callable».

3. **Практическая польза и будущее.**

   * Сейчас мы вручную зарегистрировали лишь несколько базовых методов у встроенных контейнеров. Но когда появятся классы ( `class MyClass: ...` ), ты можешь в `ClassDecl` (в `Executor`) во время объявления класса тоже добавить в `TypeRegistry` все методы, лежащие в `classDict`, и разрешать вызывать их через `__call__`. Или, например, после динамического присваивания функции в атрибут класса тебе автоматически станет доступен вызов.
   * При старте ты мог бы из «конфигурационного файла» прочитать информацию о типах, вызвать `TypeRegistry::registerType(…)` для всех типов, а потом подставить туда методы. Это даёт очень гибкую расширяемость, когда ты со временем захочешь держать сотню типов.

---

## 4. pyfunction.cpp

### 4.1 Зачем нужен этот файл

* В `object.hpp` мы объявили класс `PyFunction`, но не дали конкретных реализаций его методов.
* Кроме того, нам нужно рассказать C++-компилятору, как именно происходит вызов пользовательской функции: то есть где брать её AST, как заводить новый `Executor`, как проверять количество аргументов, как «пересоздать» локальный скоуп, как ловить `ReturnException`, и т. д.

### 4.2 Основные моменты

Откроем ключевые фрагменты `pyfunction.cpp` (сокращённо):

```cpp
#include "object.hpp"     // тут есть PyFunction, ObjectPtr, RuntimeError,…
#include "executer.hpp"   // тут Executor, ReturnException, SymbolType, Symbol и т. д.
#include <string>
#include <vector>

std::type_index PyFunction::type() const {
    return typeid(PyFunction);
}

std::string PyFunction::repr() const {
    std::ostringstream ss;
    ss << "<function " << decl->name << " at " << this << ">";
    return ss.str();
}

FuncDecl* PyFunction::getDecl() const {
    return decl;
}


PyFunction::PyFunction(const std::string &funcName,
                       FuncDecl *f,
                       std::shared_ptr<SymbolTable> scope,
                       const std::vector<std::string> &parameters,
                       std::vector<ObjectPtr> defaults)
    : name(funcName),
      decl(f),
      scope(std::move(scope)),
      posParams(parameters),
      defaultValues(std::move(defaults))
{}

// __call__: главный метод
ObjectPtr PyFunction::__call__(const std::vector<ObjectPtr> &args) {
    // 1) Создаем новый Executor. В его конструкторе он сам «зарегистрирует» встроенные функции (print, range, len, dir, enumerate).
    Executor exec;

    // 2) Создаем новый локальный скоуп внутри этого Executor:
    //    — у Executor есть поле `Scope scopes;`
    exec.scopes.enter_scope();
    //    Это означает: мы теперь в функции работаем в новой таблице символов, унаследованной от глобальной (где уже лежат print, range и т. д.)

    // 3a) Привязываем **позиционные** параметры:
    const auto &paramNames = decl->posParams;   // в AST-узле FuncDecl есть vector<string> posParams
    size_t requiredCount = paramNames.size();
    if (args.size() < requiredCount) {
        throw RuntimeError(decl->name + "() missing required positional arguments");
    }
    for (size_t i = 0; i < requiredCount; ++i) {
        Symbol s;
        s.name  = paramNames[i];
        s.type  = SymbolType::Parameter;
        s.value = args[i];      // просто кладём в value пришедший аргумент
        exec.scopes.insert(s);
    }

    // 3b) Привязываем **default**-параметры:
    const auto &declDefParams = decl->defaultParams;     // в AST: vector<pair<string, Expression*>>
    const auto &storedDefaults = this->defaultValues;    // уже заранее вычисленные в момент объявления функции
    for (size_t i = 0; i < declDefParams.size(); ++i) {
        const std::string &paramName = declDefParams[i].first;
        ObjectPtr valueToBind;
        size_t argIndex = requiredCount + i;
        if (argIndex < args.size()) {
            // если вызов `f(a, b, c)` покрывает этот параметр — берём из args
            valueToBind = args[argIndex];
        } else {
            // иначе — используем заранее вычисленное defaultValue из this->defaultValues
            valueToBind = storedDefaults[i];
        }
        Symbol s;
        s.name  = paramName;
        s.type  = SymbolType::Parameter;
        s.value = valueToBind;
        exec.scopes.insert(s);
    }

    // 3c) Если передали **слишком много** аргументов:
    size_t maxAllowed = paramNames.size() + declDefParams.size();
    if (args.size() > maxAllowed) {
        throw RuntimeError(
            std::string("TypeError: ") + decl->name + "() takes from "
            + std::to_string(paramNames.size()) + " to "
            + std::to_string(maxAllowed)
            + " positional arguments but " + std::to_string(args.size())
            + " were given"
        );
    }

    // 4) Выполняем тело функции:
    ObjectPtr returnValue = std::make_shared<PyNone>(); // default — None
    try {
        if (decl->body) {
            decl->body->accept(exec);
            // Если тело выполнено без `return`, то возвращаем `None`
        }
    }
    catch (const ReturnException &ret) {
        // Если внутри тела встретился `ReturnStat`, то там кинут `ReturnException(retValue)`,
        // и сюда попадёт. Достаем `ret.value` и подсовываем во `returnValue`.
        returnValue = ret.value;
    }

    // 5) Выходим из локальной области видимости:
    exec.scopes.leave_scope();

    return returnValue;
}
```

### 4.3 «Как это работает» на пальцах

1. **Привязывание контекста (лексического окружения).**

   * Когда мы объявили функцию `def foo(a, b=5): …`, в момент объявления мы сделали (в `Executor::visit(FuncDecl)`) примерно следующее:

     ```cpp
     // ... вычислили defaultValues для b — пусть это `[PyInt(5)]` ...
     auto fn_obj = std::make_shared<PyFunction>(
         "foo",          // имя
         &node,          // AST-узел FuncDecl
         scopes.currentTable(), // ТЕКУЩАЯ таблица символов (глобальный скоуп или где там объявляли функцию)
         node.posParams, // vector<string>{"a"}
         default_values  // vector<ObjectPtr>{ PyInt(5) }
     );
     // затем мы положили fn_obj в текущую таблицу символов под именем "foo".
     ```
   * Таким образом, внутри `PyFunction` у нас есть поле `scope`, которое хранит *именно тот shared\_ptr<SymbolTable>*, где функция была объявлена. Это и есть **лексическое окружение**.
   * Благодаря этому, когда внутри `foo` мы обратимся к какой-то переменной из внешнего контекста (например, к глобальным переменным или к переменным из родительской функции, если мы вложили объявление), при `scopes.lookup(name)` они будут найдены, потому что `scopes.currentTable()` внутри нового `Executor` при создании лежало в иерархии над этой таблицей.

2. **Создание нового `Executor`.**

   * `Executor exec;` – это «новый интерпретатор», у которого уже заре­гестрированы все билтинги (print, range, len, dir, enumerate) и стоит глобальная `scope` (в которой есть те самые билтинги).
   * Но первая же строка в `__call__` «заводит» новую локальную область: `exec.scopes.enter_scope();` → у нас получился уровень «где лежат параметры и локальные переменные» функции.

3. **Привязывание параметров.**

   * Все обязательные параметры ( `posParams` ) кладутся буквально из `args[0..requiredCount-1]` в эту новую таблицу.
   * Затем, если есть default-параметры (например, `b=5`), мы либо берём из `args` (если при вызове кто-то передал второй аргумент), либо берём заранее вычисленный `5`.
   * Мы проверяем, что `args.size()` не слишком велико (иначе бросаем ошибку типа «function takes X arguments but Y were given»).

4. **Выполнение полного тела.**

   * `decl->body->accept(exec);` запускает `Executor::visit(...)` для всего тела в AST.
   * Если где-то в теле встретится `return someExpr;`, то в `Executor::visit(ReturnStat)` мы вычислим `someExpr`, возьмём его из стека и **кинем исключение** `throw ReturnException{value}`.
   * Мы ловим это исключение в `__call__` → достаём из него `ret.value` и подсовываем его как результат.
   * Если `return` вообще не встретился — код тела выполнился от начала до конца, а у нас в `returnValue` до этого стоял `PyNone()`. Значит, функция вернула `None`, как и в Python.

5. **Выход из области видимости.**

   * `exec.scopes.leave_scope();` → удаляется локальный скоуп с параметрами и локальными переменными, остаётся прежняя (глобальная).
   * Мы возвращаем `returnValue`.

6. **Что дальше.**

   * Объект `PyFunction` живёт дальше (если он был сохранён где-то в глобальном скоупе).
   * Каждый раз, когда кто-то вызовет `foo(…)`, будет создаваться новый `Executor`, новый локальный скоуп, и параметры либо default-ы вновь «вгонятся» в таблицу.

Таким образом `PyFunction::__call__` – это вся механика «как работает вызов функции в Python»: раскрутка лексического окружения, bind-параметров, исполнение тела, ловля `return`, возврат результата.

---

## 5. executer.cpp

Это самый объёмный файл, потому что здесь реализован «сердечный» обход AST (паттерн Visitor) и логика выполнения практически всех языковых конструкций: от арифметики и вызовов до циклов, условных, генераторов списков, классов и лямбд. Я расскажу по методам (главное, чтобы ты мог преподавателю показать, что понимаешь, зачем каждый кусочек).

> **Замечание.** В тексте ниже я называю «посещением» — вызов метода `visit(...)` у `Executor`.

### 5.1 Вспомогательные функции

```cpp
auto is_truthy(ObjectPtr &obj) { … }
```

* Определяет «правду/ложь» в духе Python:

  * `PyBool(false)` → ложь, `PyBool(true)` → истина;
  * `PyInt(0)` и `PyFloat(0.0)` → ложь; всё остальное число → истина;
  * `PyString("")` → ложь, непустая строка → истина;
  * `PyList`, `PyDict`, `PySet` пустые → ложь, непустые → истина;
  * `PyNone` → ложь;
  * «Любые другие объекты» (функции, классы, экземпляры и т. д.) → истина.
* Используется в `if`, `while`, логических операциях, `not`, `and`, `or`, операторе `in` и т. д.

```cpp
auto deduceTypeName(ObjectPtr &obj) {
    if (dynamic_pointer_cast<PyInt>(obj))    return "int";
    if (dynamic_pointer_cast<PyFloat>(obj))  return "float";
    if (dynamic_pointer_cast<PyBool>(obj))   return "bool";
    if (dynamic_pointer_cast<PyString>(obj)) return "str";
    if (dynamic_pointer_cast<PyList>(obj))   return "list";
    if (dynamic_pointer_cast<PyDict>(obj))   return "dict";
    if (dynamic_pointer_cast<PySet>(obj))    return "set";
    if (dynamic_pointer_cast<PyNone>(obj))   return "NoneType";
    if (dynamic_pointer_cast<PyBuiltinFunction>(obj)) return "builtin_function_or_method";
    if (dynamic_pointer_cast<PyFunction>(obj)) return "function";
    return "object";
}
```

* Быстрый способ получить строковое название типа для формирования сообщений об ошибках (например, «TypeError: 'int' object is not iterable»).

### 5.2 Конструктор `Executor::Executor()`

```cpp
Executor::Executor() : scopes(), reporter() {
    TypeRegistry::instance().registerBuiltins();

    // Регистрация print:
    auto print_fn = make_shared<PyBuiltinFunction>(
        "print",
        [](const std::vector<ObjectPtr>& args) {
            for (auto &a : args) std::cout << a->repr() << " ";
            std::cout << std::endl;
            return make_shared<PyNone>();
        }
    );
    scopes.insert(Symbol{"print", SymbolType::BuiltinFunction, print_fn, nullptr});

    // Регистрация range(n):
    auto range_fn = make_shared<PyBuiltinFunction>(
        "range",
        [](const std::vector<ObjectPtr>& args) {
            auto arg0 = dynamic_pointer_cast<PyInt>(args[0]);
            if (!arg0) throw RuntimeError("TypeError: range() argument must be int");
            int n = arg0->get();
            vector<ObjectPtr> elems; elems.reserve(n);
            for (int i = 0; i < n; ++i) elems.push_back(make_shared<PyInt>(i));
            return make_shared<PyList>(move(elems));
        }
    );
    scopes.insert(Symbol{"range", SymbolType::BuiltinFunction, range_fn, nullptr});

    // Регистрация len(obj):
    auto len_fn = make_shared<PyBuiltinFunction>(
        "len",
        [](const std::vector<ObjectPtr>& args) {
            if (args.size() != 1) {
                throw RuntimeError("TypeError: len() takes exactly one argument");
            }
            ObjectPtr obj = args[0];
            if (auto s = dynamic_pointer_cast<PyString>(obj)) {
                return make_shared<PyInt>((int)s->get().size());
            }
            if (auto lst = dynamic_pointer_cast<PyList>(obj)) {
                return make_shared<PyInt>((int)lst->getElements().size());
            }
            if (auto d = dynamic_pointer_cast<PyDict>(obj)) {
                return make_shared<PyInt>((int)d->getItems().size());
            }
            if (auto st = dynamic_pointer_cast<PySet>(obj)) {
                return make_shared<PyInt>((int)st->getElements().size());
            }
            // Если другой тип:
            std::string tname = deduceTypeName(obj);
            throw RuntimeError("TypeError: object of type '" + tname + "' has no len()");
        }
    );
    scopes.insert(Symbol{"len", SymbolType::BuiltinFunction, len_fn, nullptr});

    // Регистрация dir(obj):
    auto dir_fn = make_shared<PyBuiltinFunction>(
        "dir",
        [](const std::vector<ObjectPtr>& args) {
            if (args.size() != 1) {
                throw RuntimeError("TypeError: dir() takes exactly one argument");
            }
            ObjectPtr obj = args[0];
            vector<ObjectPtr> names;
            if (auto lst = dynamic_pointer_cast<PyList>(obj)) {
                int n = (int)lst->getElements().size();
                for (int i = 0; i < n; ++i) {
                    names.push_back(make_shared<PyString>(std::to_string(i)));
                }
                return make_shared<PyList>(move(names));
            }
            if (auto d = dynamic_pointer_cast<PyDict>(obj)) {
                for (auto &kv : d->getItems()) {
                    names.push_back(make_shared<PyString>(kv.first->repr()));
                }
                return make_shared<PyList>(move(names));
            }
            if (auto s = dynamic_pointer_cast<PyString>(obj)) {
                for (char c : s->get()) {
                    names.push_back(make_shared<PyString>(string(1, c)));
                }
                return make_shared<PyList>(move(names));
            }
            if (auto st = dynamic_pointer_cast<PySet>(obj)) {
                for (auto &el : st->getElements()) {
                    names.push_back(make_shared<PyString>(el->repr()));
                }
                return make_shared<PyList>(move(names));
            }
            // Другие типы → пустой список
            return make_shared<PyList>(vector<ObjectPtr>{});
        }
    );
    scopes.insert(Symbol{"dir", SymbolType::BuiltinFunction, dir_fn, nullptr});

    // Регистрация enumerate(iterable):
    auto enumerate_fn = make_shared<PyBuiltinFunction>(
        "enumerate",
        [](const std::vector<ObjectPtr>& args) {
            if (args.size() != 1) {
                throw RuntimeError("TypeError: enumerate() takes exactly one argument");
            }
            ObjectPtr obj = args[0];
            vector<ObjectPtr> result;
            if (auto lst = dynamic_pointer_cast<PyList>(obj)) {
                const auto &elems = lst->getElements();
                for (size_t i = 0; i < elems.size(); ++i) {
                    vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(make_shared<PyInt>((int)i));
                    pairVec.push_back(elems[i]);
                    result.push_back(make_shared<PyList>(move(pairVec)));
                }
                return make_shared<PyList>(move(result));
            }
            if (auto s = dynamic_pointer_cast<PyString>(obj)) {
                const string &str = s->get();
                for (size_t i = 0; i < str.size(); ++i) {
                    vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(make_shared<PyInt>((int)i));
                    pairVec.push_back(make_shared<PyString>(string(1, str[i])));
                    result.push_back(make_shared<PyList>(move(pairVec)));
                }
                return make_shared<PyList>(move(result));
            }
            if (auto d = dynamic_pointer_cast<PyDict>(obj)) {
                const auto &items = d->getItems();
                size_t idx = 0;
                for (auto &kv : items) {
                    vector<ObjectPtr> pairVec;
                    pairVec.reserve(2);
                    pairVec.push_back(make_shared<PyInt>((int)idx));
                    pairVec.push_back(kv.first);
                    result.push_back(make_shared<PyList>(move(pairVec)));
                    ++idx;
                }
                return make_shared<PyList>(move(result));
            }
            // Иначе — TypeError
            string tname = deduceTypeName(obj);
            throw RuntimeError("TypeError: '" + tname + "' object is not iterable");
        }
    );
    scopes.insert(Symbol{"enumerate", SymbolType::BuiltinFunction, enumerate_fn, nullptr});
}
```

* **Итог.** После конструктора у нас в глобальном скоупе ( `scopes.currentTable()` ) лежат имена:

  ```
  print → PyBuiltinFunction("print",…)
  range → PyBuiltinFunction("range",…)
  len → PyBuiltinFunction("len",…)
  dir → PyBuiltinFunction("dir",…)
  enumerate → PyBuiltinFunction("enumerate",…)
  ```

  – то есть всё готово к тому, чтобы в любом месте программы можно было написать `print("hello")`, `range(5)`, `len([1,2,3])`, `dir(my_str)`, `enumerate(my_list)`.

---

### 5.3 `Executor::evaluate` и `Executor::execute`

```cpp
std::shared_ptr<Object> Executor::evaluate(Expression &expr) {
    expr.accept(*this);
    return pop_value();
}
```

* Удобный метод, чтобы «извне» (например, в `PyFunction::__call__`) быстро вычислить какие-то выражения:

  1. Вызвать `accept` для корня AST этого выражения,
  2. забрать единственное значение из внутреннего стека `valueStack` (где `Executor` хранит промежуточные результаты) и вернуть его.

```cpp
void Executor::execute(TransUnit &unit) {
    try {
        unit.accept(*this);
    }
    catch (const RuntimeError &err) {
        std::cerr << "RuntimeError: " << err.what() << "\n";
        return;
    }

    if (reporter.has_errors()) {
        reporter.print_errors();
    }
}
```

* Этот метод вызывается из «точки входа» интерпретатора (например, из `main` после парсинга).
* `unit` — это корень AST всего модуля (TransUnit обычно хранит список `units`, каждый из которых может быть либо инструкцией (Stat), либо другими включениями).
* Если где-то в процессе бросилось `RuntimeError` — мы его ловим тут и печатаем в stderr, а выполнение «стыдливо» останавливается.
* Если же каких-то ошибок (например, семантических) накопилось в `reporter` (например, дубли имен параметров в `FuncDecl`), после этого мы выводим их.

---

### 5.4 `Executor::visit(TransUnit &node)`

```cpp
void Executor::visit(TransUnit &node) {
    for (auto &unit : node.units) {
        unit->accept(*this);
    }
}
```

* Просто проходим по списку всех «юнитов» (каждый `unit` — это `unique_ptr<Stat>` или `unique_ptr<Decl>`, смотря как устроен AST).
* Ничего особенного: это аналог «тела модуля» в Python, когда ты исполняешь инструкции сверху вниз.

---

### 5.5 `Executor::visit(FuncDecl &node)`

```cpp
void Executor::visit(FuncDecl &node) {
    // 1) Проверка дубликатов имён параметров:
    {
        unordered_set<string> seen;
        // posParams
        for (auto &name : node.posParams) {
            if (!seen.insert(name).second) {
                reporter.add_error("Line X: duplicate parameter name '" + name + "'");
            }
        }
        // defaultParams
        for (auto &pr : node.defaultParams) {
            const string &name = pr.first;
            if (!seen.insert(name).second) {
                reporter.add_error("Line X: duplicate parameter name '" + name + "'");
            }
        }
    }
    if (reporter.has_errors()) return;

    // 2) Вычисляем defaultValues **сразу при объявлении**:
    vector<ObjectPtr> default_values;
    for (auto &pr : node.defaultParams) {
        Expression *expr = pr.second.get();
        if (expr) {
            expr->accept(*this);
            ObjectPtr value = pop_value();
            default_values.push_back(value);
        } else {
            default_values.push_back(make_shared<PyNone>());
        }
    }

    // 3) Создаём PyFunction и кладём его в таблицу символов:
    auto fn_obj = make_shared<PyFunction>(
        node.name,
        &node,
        scopes.currentTable(),
        node.posParams,
        move(default_values)
    );
    Symbol sym;
    sym.name  = node.name;
    sym.type  = SymbolType::Function;
    sym.value = fn_obj;
    sym.decl  = &node;

    if (auto existing = scopes.lookup_local(node.name)) {
        *existing = sym; // перезаписываем
    } else {
        scopes.insert(sym);
    }
}
```

* **Разбираем по шагам:**

  1. **Проверка дубликатов параметров.** Если в `def f(a, b, a): …` вдруг два раза упоминали `a` — мы это сразу ловим и заносим в `reporter`.
  2. **Вычисляем default-значения** прямо здесь, когда объявляем функцию, а не когда вызываем.

     * Почему так? Потому что Python-семантика гласит: дефолт «фризится» в момент объявления.
     * Например:

       ```python
       def f(x=a): pass
       a = 5
       f()  # возьмёт старое значение a, которое было в момент объявления
       ```
  3. **Создаём `PyFunction`**:

     * `node.name` (имя функции);
     * `&node` (указатель на AST, чтобы знать posParams, defaultParams, тело);
     * `scopes.currentTable()` (лексический контекст, чтобы замыкать переменные);
     * `node.posParams` (список имён обязательных параметров);
     * ранее вычисленные `default_values`.
  4. **Кладём `PyFunction` в таблицу символов** текущей области видимости под именем `node.name`.

     * Если там уже есть символ с таким именем в локальном скоупе — перезаписываем. Иначе вставляем новый.

* **Важно.** Мы не входим внутрь тела функции (нет ни `enter_scope()`, ни `leave_scope()`) на этапе объявления. Тело будет исполнено только при реальном вызове `foo(...)` в `PyFunction::__call__`.

---

### 5.6 `Executor::visit(BlockStat &node)`

```cpp
void Executor::visit(BlockStat &node) {
    for (auto &stat : node.statements) {
        stat->accept(*this);
        if (reporter.has_errors()) return;
    }
}
```

* Просто подряд визитим все инструкции внутри блока ( `{ … }` или тело `if`/`while`).
* Если где-то внутри появятся ошибки, они добавятся в `reporter`, и мы сразу выйдем (чтобы не плодить лишние сбои).

---

### 5.7 `Executor::visit(ExprStat &node)`

```cpp
void Executor::visit(ExprStat &node) {
    if (node.expr) {
        node.expr->accept(*this);
        pop_value();  // избавляемся от результата, т.к. «выражение как инструкция» не возвращает ничего
    }
}
```

* Когда у нас в тексте стоит «выражение» без присваивания, это всё равно надо «посчитать», потому что могут быть вызовы функций (например, `print(x)` или просто `foo()`).
* Но после того, как мы получили `ObjectPtr` результата, нам он не нужен — мы его «сбрасываем».

---

### 5.8 `Executor::visit(AssignStat &node)`

```cpp
void Executor::visit(AssignStat &node) {
    // 1) Вычисляем правую часть (если есть), иначе считаем это равным PyNone
    if (node.right) {
        node.right->accept(*this);
    } else {
        push_value(make_shared<PyNone>());
    }
    ObjectPtr right_val = pop_value();

    // 2) Разбираем левую часть:
    // 2.1) Если это IdExpr (просто имя переменной):
    if (auto id = dynamic_cast<IdExpr*>(node.left.get())) {
        string var = id->name;
        if (!scopes.lookup_local(var)) {
            Symbol sym;
            sym.name = var;
            sym.type = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl = &node;
            scopes.insert(sym);
        }
        Symbol *sym = scopes.lookup_local(var);
        sym->value = right_val;
        return;
    }

    // 2.2) Если индексное присваивание (IndexExpr), например list[i] = X или dict[key] = X:
    if (auto idx = dynamic_cast<IndexExpr*>(node.left.get())) {
        idx->base->accept(*this);
        ObjectPtr base = pop_value();
        idx->index->accept(*this);
        ObjectPtr index = pop_value();
        try {
            base->__setitem__(index, right_val);
        } catch (const RuntimeError &err) {
            throw;
        }
        return;
    }

    // 2.3) Если атрибутное присваивание (AttributeExpr), например obj.field = X:
    if (auto attr = dynamic_cast<AttributeExpr*>(node.left.get())) {
        attr->obj->accept(*this);
        ObjectPtr base = pop_value();
        string field = attr->name;
        try {
            base->__setattr__(field, right_val);
        } catch (const RuntimeError &err) {
            throw;
        }
        return;
    }

    // 2.4) Во всех остальных случаях — недопустимая цель:
    throw RuntimeError("Line X: invalid assignment target");
}
```

* **Случай 2.1: обычная переменная.**

  * Если её раньше не объявляли локально ( `lookup_local(var) == nullptr` ), создаём новый `Symbol` и вставляем в текущий (локальный) скоуп.
  * Затем просто обновляем `sym->value = right_val`.
  * Если переменная была объявлена (например, мы внутри функции уже её использовали), мы перезаписываем старое значение.

* **Случай 2.2: присваивание по индексу** (типа `list[i] = value` или `dict[key] = value`).

  * Сначала нужно вычислить «базу» (`list` или `dict`): `idx->base->accept(*this)` → `pop_value()` → `base`.
  * Потом «ключ» или «индекс»: `idx->index->accept(*this)` → `pop_value()` → `index`.
  * И вызываем `base->__setitem__(index, right_val)`.

    * Если `base` — `PyList`, то этот метод проверит, что `index` целое, и установит новый элемент или упадёт, если индекс выходит за пределы.
    * Если `base` не поддерживает `__setitem__` (например, строка), унаследованный метод выбросит `RuntimeError("object does not support item assignment")`.

* **Случай 2.3: атрибутное присваивание** (например, `obj.field = X`).

  * Вычисляем объект `obj`: `attr->obj->accept(*this)` → `pop_value()` → `base`.
  * И вызываем `base->__setattr__(field, right_val)`.

    * Если `base` — экземпляр нашего `PyInstance`, у него `__setattr__` просто положит пару `(PyString(field)->repr(), right_val)` в `instanceDict`.
    * Если `base` — `PyClass`, то положит в `classDict`.
    * Если `base` не поддерживает атрибуты вообще (например, `int`), унаследованное `__setattr__` кинет Run-time Error.

* **Во всех остальных случаях** (например, `5 = 3`, `{1:2} = 5`, `(a,b) = 3`, которые синтаксически могли быть пропущены парсером) — бросаем `invalid assignment target`.

---

### 5.9 `Executor::visit(IdExpr &node)`

```cpp
void Executor::visit(IdExpr &node) {
    Symbol* sym = scopes.lookup(node.name);
    if (!sym) {
        throw RuntimeError("Line X: name '" + node.name + "' is not defined");
    }
    if (!sym->value) {
        throw RuntimeError("Line X: variable '" + node.name + "' referenced before assignment");
    }
    push_value(sym->value);
}
```

* Когда встречаем идентификатор `x`, надо найти его в цепочке таблиц символов ( `lookup(node.name)` ).

  * Если не нашли – ошибка времени выполнения «name 'x' is not defined».
  * Если нашли, но `sym->value == nullptr` (т. е. ранее объявили, но ещё не присвоили) – «referenced before assignment».
  * Иначе просто кладём в стек значений `sym->value`.

---

### 5.10 `Executor::visit(BinaryExpr &node)`

```cpp
void Executor::visit(BinaryExpr &node) {
    node.left->accept(*this);
    ObjectPtr leftVal = pop_value();
    node.right->accept(*this);
    ObjectPtr rightVal = pop_value();

    const std::string &op = node.op;
    ObjectPtr result;

    if (op == "+") {
        result = leftVal->__add__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "-") {
        result = leftVal->__sub__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "*") {
        result = leftVal->__mul__(rightVal);
        push_value(result);
        return;
    }
    else if (op == "/") {
        result = leftVal->__div__(rightVal);
        push_value(result);
        return;
    }

    // Сравнения:
    else if (op == "==" || op == "!=" || op == "<" || op == ">" || op == "<=" || op == ">=") {
        string lhs = leftVal->repr();
        string rhs = rightVal->repr();
        bool comp = false;
        if (op == "==") comp = (lhs == rhs);
        else if (op == "!=") comp = (lhs != rhs);
        else if (op == "<")  comp = (lhs < rhs);
        else if (op == ">")  comp = (lhs > rhs);
        else if (op == "<=") comp = (lhs <= rhs);
        else if (op == ">=") comp = (lhs >= rhs);
        push_value(make_shared<PyBool>(comp));
        return;
    }

    // Логические and / or
    else if (op == "and" || op == "or") {
        bool leftTruthy = is_truthy(leftVal);
        if (op == "and") {
            if (!leftTruthy) push_value(leftVal);
            else push_value(rightVal);
            return;
        } else {
            if (leftTruthy) push_value(leftVal);
            else push_value(rightVal);
            return;
        }
    }

    // in / not in
    else if (op == "in" || op == "not in") {
        bool containsResult = false;
        try {
            containsResult = rightVal->__contains__(leftVal);
        } catch (const RuntimeError &err) {
            throw RuntimeError("Line X TypeError: " + err.what());
        }
        if (op == "not in") containsResult = !containsResult;
        push_value(make_shared<PyBool>(containsResult));
        return;
    }

    throw RuntimeError("Line X: unsupported binary operator '" + op + "'");
}
```

* **Алгебраические операторы (`+ - * /`).**

  * Сначала вычисляем левую часть, получаем `leftVal`. Затем правую часть, получаем `rightVal`.
  * Вызываем нужный виртуальный метод. Например, `leftVal->__add__(rightVal)`.
  * Если типы непогодятся (например, `list + 5`), внутри метода `__add__` у `PyList` мы бросим `RuntimeError("can only concatenate list (not 'int') to list")`, и это «пробросится» наверх, загонит нас в `catch (RuntimeError&)` в `execute()` и выведет в stderr.
* **Операторы сравнения (`== != < > <= >=`).**

  * Мы тривиально сравниваем `repr()` двух объектов. Это не совсем как в Python (обычно сравнение списков смотрит глубоко на элементы), но чтобы упростить задачу, достаточно.
  * Результат — новый `PyBool`.
* **Логические `and/or`.**

  * Здесь особенность Python:

    * `a and b`: если `a` ложь, результатом выражения `a and b` является сам `a` (не `False`, а именно объект `a`), иначе результатом — `b`.
    * `a or b`: если `a` истинно, результатом — `a`, иначе — `b`.
  * Мы реализовали ровно эту логику:

    ```cpp
    bool leftTruthy = is_truthy(leftVal);
    if (op == "and") {
        if (!leftTruthy) push_value(leftVal);
        else push_value(rightVal);
    }
    else { // or
        if (leftTruthy) push_value(leftVal);
        else push_value(rightVal);
    }
    ```
* **Операторы `in` / `not in`:**

  * Мы вызываем `rightVal->__contains__(leftVal)`.
  * Если тип `rightVal` не поддерживает `__contains__` (например, `int`), то метод внутри даст `RuntimeError("object is not iterable")`, мы ловим его и переворачиваем в сообщение «Line X TypeError: object is not iterable».
  * Результат помещаем в `PyBool`. Если `not in`, инвертируем его.

---

### 5.11 `Executor::visit(UnaryExpr &node)`

```cpp
void Executor::visit(UnaryExpr &node) {
    if (node.operand) node.operand->accept(*this);
    else throw RuntimeError("Line X: missing operand for unary operator");

    ObjectPtr operandVal = pop_value();
    const std::string &op = node.op;

    if (op == "+") {
        if (auto pint = dynamic_pointer_cast<PyInt>(operandVal)) {
            push_value(pint);
            return;
        }
        if (auto pfloat = dynamic_pointer_cast<PyFloat>(operandVal)) {
            push_value(pfloat);
            return;
        }
        if (auto pbool = dynamic_pointer_cast<PyBool>(operandVal)) {
            int intval = pbool->get() ? 1 : 0;
            push_value(make_shared<PyInt>(intval));
            return;
        }
        string tname = deduceTypeName(operandVal);
        throw RuntimeError("Line X TypeError: bad operand type for unary +: '" + tname + "'");
    }
    else if (op == "-") {
        if (auto pint = dynamic_pointer_cast<PyInt>(operandVal)) {
            push_value(make_shared<PyInt>(-pint->get()));
            return;
        }
        if (auto pfloat = dynamic_pointer_cast<PyFloat>(operandVal)) {
            push_value(make_shared<PyFloat>(-pfloat->get()));
            return;
        }
        if (auto pbool = dynamic_pointer_cast<PyBool>(operandVal)) {
            int intval = pbool->get() ? 1 : 0;
            push_value(make_shared<PyInt>(-intval));
            return;
        }
        string tname = deduceTypeName(operandVal);
        throw RuntimeError("Line X TypeError: bad operand type for unary -: '" + tname + "'");
    }
    else if (op == "not") {
        bool truth = is_truthy(operandVal);
        push_value(make_shared<PyBool>(!truth));
        return;
    }
    else {
        throw RuntimeError("Line X SyntaxError: invalid unary operator '" + op + "'");
    }
}
```

* **Унарный `+`.**

  * В Python «`+x`» означает «сколько-то скопировать знак» — обычно просто возвращают само `x`, если это число.
  * Мы поддерживаем `PyInt`, `PyFloat`, `PyBool` (переводим его в `1` или `0` → `PyInt`).
  * Для остальных типов — «TypeError: bad operand type for unary +: 'str'» и т. д.
* **Унарный `-`.**

  * Аналогично, но если `PyBool` → превращаем в `PyInt` с минусом ( `-True` → `-1`, `-False` → `0` ).
  * Если `PyInt` → создаём новый `PyInt(- value)`.
  * Если `PyFloat` → `PyFloat(-value)`.
  * Иначе — «TypeError: bad operand type for unary -».
* **Логический `not`.**

  * Просто «!truthiness» и возвращаем `PyBool`.
* **Если какой-то странный оператор, не `+`, не `-`, не `not` — бросаем `SyntaxError`** (в нашем случае `RuntimeError`, но с соответствующим текстом).

---

### 5.12 `Executor::visit(CallExpr &node)`

```cpp
void Executor::visit(CallExpr &node) {
    // 1) Вычисляем «callee»: может быть IdExpr (имя функции),
    // или более сложное выражение, возвращающее объект, у которого есть __call__.
    node.caller->accept(*this);
    ObjectPtr callee = pop_value();

    // 2) Вычисляем все аргументы по порядку:
    vector<ObjectPtr> args;
    args.reserve(node.arguments.size());
    for (auto &argExpr : node.arguments) {
        argExpr->accept(*this);
        args.push_back(pop_value());
    }

    // 3) Проверяем, является ли callee «встроенной функцией»:
    if (auto builtinFn = dynamic_pointer_cast<PyBuiltinFunction>(callee)) {
        try {
            ObjectPtr result = builtinFn->__call__(args);
            push_value(result);
            return;
        } catch (const RuntimeError &err) {
            throw RuntimeError("Line X " + err.what());
        }
    }

    // 4) Если callee — пользовательская функция (PyFunction):
    if (auto userFn = dynamic_pointer_cast<PyFunction>(callee)) {
        FuncDecl *decl = userFn->getDecl();
        size_t provided = args.size();
        size_t requiredPos = decl->posParams.size();
        size_t defaultCount = decl->defaultParams.size();

        // 4.1) Проверяем количество аргументов:
        if (provided < requiredPos) {
            // Собираем список «пропущенных» имен
            // Формируем сообщение с missing X required positional arguments
            // Строим имена через «and»
            throw RuntimeError("Line X TypeError: f() missing ...");
        }
        if (provided > requiredPos + defaultCount) {
            throw RuntimeError("Line X TypeError: f() takes from ... to ... positional arguments but ... were given");
        }

        // 4.2) Входим в новую область видимости:
        scopes.enter_scope();

        // 4.3) Привязываем позиционные:
        for (size_t i = 0; i < requiredPos; ++i) {
            Symbol sym;
            sym.name = decl->posParams[i];
            sym.type = SymbolType::Parameter;
            sym.value = args[i];
            sym.decl = decl;
            scopes.insert(sym);
        }

        // 4.4) Привязываем default-параметры:
        const auto &defParams = decl->defaultParams;        // AST
        const auto &defValues = userFn->getDefaultValues(); // PyFunction
        for (size_t i = 0; i < defParams.size(); ++i) {
            const string &paramName = defParams[i].first;
            ObjectPtr valueToBind;
            size_t argIndex = requiredPos + i;
            if (argIndex < provided) {
                valueToBind = args[argIndex];
            } else {
                valueToBind = defValues[i];
            }
            Symbol sym;
            sym.name = paramName;
            sym.type = SymbolType::Parameter;
            sym.value = valueToBind;
            sym.decl = decl;
            scopes.insert(sym);
        }

        // 4.5) Выполняем тело функции:
        ObjectPtr returnValue = make_shared<PyNone>();
        try {
            if (decl->body) decl->body->accept(*this);
        } catch (const ReturnException &ret) {
            returnValue = ret.value;
        }

        // 4.6) Выходим из области видимости:
        scopes.leave_scope();

        push_value(returnValue);
        return;
    }

    // 5) Если callee не встроенная и не пользовательская функция,
    //    пробуем вызвать у него virtual __call__:
    try {
        ObjectPtr result = callee->__call__(args);
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        throw RuntimeError("Line X TypeError: " + err.what());
    }
}
```

* **Порядок важен.** Сначала проверяем `PyBuiltinFunction`, потом `PyFunction`, потом любой объект, у которого переопределён `__call__`. Если ни один из вариантов не подошёл, кидаем «TypeError: object is not callable».

* При вызове `PyFunction` мы дублируем логику, описанную в `pyfunction.cpp`, с той лишь разницей, что здесь мы не создаём нового `Executor`, а используем *самого себя* (тот же `Executor`) для исполнения тела. `PyFunction::__call__` в `pyfunction.cpp` создаёт новый `Executor`, а здесь мы делаем inline-версию:

  1. проверка числа аргументов,
  2. `enter_scope()`,
  3. bind posParams + defaultParams,
  4. `decl->body->accept(*this)` (то есть наши собственные visit-команды рекурсивно выполняют тело), ловим `ReturnException`,
  5. `leave_scope()`,
  6. `push_value(returnValue)`.

* **Разница с `pyfunction.cpp`.** В `pyfunction.cpp` мы вообще создавали новый объект `Executor exec;`. Здесь мы не создаём новый интерпретатор, потому что мы уже *находимся* внутри обходчика AST ( `Executor` ), и у него есть `scopes`, `valueStack` и т. д. Нам надо подменить лишь скоуп. После окончания вызова мы возвращаемся в старую область видимости.

---

### 5.13 `Executor::visit(ReturnStat &node)`

```cpp
void Executor::visit(ReturnStat &node) {
    if (node.expr) {
        node.expr->accept(*this);
        ObjectPtr retVal = pop_value();
        throw ReturnException{retVal};
    } else {
        throw ReturnException{ make_shared<PyNone>() };
    }
}
```

* Когда мы встречаем оператор `return expr`, мы сразу бросаем исключение `ReturnException{value}`.

  * Это нужно, чтобы «внезапно» выскочить из глубины всех вложенных блоков и вернуться к месту вызова функции (в `PyFunction::__call__` или в `Executor::visit(CallExpr)`).
  * Если `expr` нет, просто `return None` → кидаем `ReturnException{None}`.
* **Интерпретаторы Python обычно делают тот же трюк** (написать маленький `throw` или установить какой-то флаг для немедленного выхода из функции).

---

### 5.14 `Executor::visit(IndexExpr &node)`

```cpp
void Executor::visit(IndexExpr &node) {
    node.base->accept(*this);
    ObjectPtr baseVal = pop_value();

    node.index->accept(*this);
    ObjectPtr indexVal = pop_value();

    // Если строка:
    if (auto strObj = dynamic_pointer_cast<PyString>(baseVal)) {
        int idx;
        if (auto intIdx = dynamic_pointer_cast<PyInt>(indexVal)) {
            idx = intIdx->get();
        }
        else if (auto boolIdx = dynamic_pointer_cast<PyBool>(indexVal)) {
            idx = boolIdx->get() ? 1 : 0;
        }
        else {
            throw RuntimeError("Line X TypeError: string indices must be integers");
        }

        const auto &s = strObj->get();
        int length = (int)s.size();
        if (idx < 0) idx += length;
        if (idx < 0 || idx >= length) {
            throw RuntimeError("Line X IndexError: string index out of range");
        }
        push_value(make_shared<PyString>(string(1, s[idx])));
        return;
    }

    // Если список:
    if (auto listObj = dynamic_pointer_cast<PyList>(baseVal)) {
        int idx;
        if (auto intIdx = dynamic_pointer_cast<PyInt>(indexVal)) {
            idx = intIdx->get();
        }
        else if (auto boolIdx = dynamic_pointer_cast<PyBool>(indexVal)) {
            idx = boolIdx->get() ? 1 : 0;
        }
        else {
            throw RuntimeError("Line X TypeError: list indices must be integers");
        }

        int length = (int)listObj->getElements().size();
        if (idx < 0) idx += length;
        if (idx < 0 || idx >= length) {
            throw RuntimeError("Line X IndexError: list index out of range");
        }
        push_value(listObj->getElements()[idx]);
        return;
    }

    // Если словарь:
    if (auto dictObj = dynamic_pointer_cast<PyDict>(baseVal)) {
        try {
            ObjectPtr result = dictObj->__getitem__(indexVal);
            push_value(result);
            return;
        }
        catch (const RuntimeError &err) {
            throw RuntimeError("Line X " + err.what());
        }
    }

    // Если множество:
    if (dynamic_pointer_cast<PySet>(baseVal)) {
        throw RuntimeError("Line X TypeError: 'set' object is not subscriptable");
    }

    // Для прочих типов пробуем их __getitem__:
    try {
        ObjectPtr result = baseVal->__getitem__(indexVal);
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        throw RuntimeError("Line X TypeError: " + err.what());
    }
}
```

* **Строки.**

  * Индекс должен быть `PyInt` (или `PyBool`, который превращается в `0`/`1`).
  * Поддержка отрицательных индексов (`idx += length`).
  * Диапазонная проверка и `IndexError` при выходе за пределы.
  * Возвращается «символ» как `PyString` длины 1.

* **Списки.**

  * Почти то же самое, только возвращается элемент списка (`elems[idx]`).

* **Словари.**

  * Любой ключ (может быть `PyString`, `PyInt`, или даже кортеж, если бы мы поддерживали), просто передаём в `__getitem__` и возвращаем результат (если ключа нет — `KeyError`).

* **Множества.**

  * `set` нельзя индексировать → сразу «TypeError: 'set' object is not subscriptable».

* **Другие объекты.**

  * Попытка вызвать метод `__getitem__` напрямую. Если класс его не переопределил – бросит `RuntimeError("object is not subscriptable")`.

---

### 5.15 `Executor::visit(AttributeExpr &node)`

```cpp
void Executor::visit(AttributeExpr &node) {
    node.obj->accept(*this);
    ObjectPtr baseVal = pop_value();

    const string &attrName = node.name;
    try {
        ObjectPtr result = baseVal->__getattr__(attrName);
        push_value(result);
        return;
    }
    catch (const RuntimeError &err) {
        string typeName = deduceTypeName(baseVal);
        throw RuntimeError("Line X TypeError: '" + typeName + "' object has no attribute '" + attrName + "'");
    }
}
```

* **Как работает:**

  1. Сначала вычисляем «что-то слева от точки» → получаем `baseVal`.
  2. Пытаемся `baseVal->__getattr__(attrName)`.

     * Если `baseVal` — экземпляр `PyInstance`, он сначала смотрит в `instanceDict`, потом у `classPtr->__getattr__`.
     * Если `baseVal` — `PyClass`, он смотрит в своё `classDict`, потом у родителя.
     * Если `baseVal` — что-то другое (например, `PyInt`), `__getattr__` базовый бросит `RuntimeError("object has no attribute 'foo'")`.
  3. Если `__getattr__` у `baseVal` прошёл успешно — мы кладём результат в стек;
     если упал с `RuntimeError`, мы «оборачиваем» его в сообщение «TypeError: 'TYPE' object has no attribute 'name'».

* **Прикладное применение.**

  * Это позволяет цепочку выражений `obj.field1.field2()` — каждый раз вызов `visit(AttributeExpr)` вытаскивает следующий атрибут из `instanceDict` или `classDict` (и рябь родителей, если наследование).
  * Если, скажем, метод в классе был определён как `def foo(self, x): …`, то `baseVal->__getattr__("foo")` вернёт `PyFunction` (или, точнее, `PyFunction` внутри класса). Затем, когда это значение попадёт в `CallExpr`, исполнится способ вызова `PyFunction`, который внутри вторым аргументом примет `self` и прочие параметры.

---

### 5.16 `Executor::visit(TernaryExpr &node)`

```cpp
void Executor::visit(TernaryExpr &node) {
    node.condition->accept(*this);
    ObjectPtr condVal = pop_value();
    bool condIsTrue = is_truthy(condVal);

    if (condIsTrue) {
        node.trueExpr->accept(*this);
        ObjectPtr trueResult = pop_value();
        push_value(trueResult);
    } else {
        node.falseExpr->accept(*this);
        ObjectPtr falseResult = pop_value();
        push_value(falseResult);
    }
}
```

* **Логика оператора `<expr1> if <condition> else <expr2>`**:

  1. Вычисляем условие → проверяем через `is_truthy`.
  2. Если истина — выполняем `trueExpr`, иначе — `falseExpr`.
  3. Результат выкладываем в стек.

---

### 5.17 `Executor::visit(LiteralExpr &node)`

```cpp
void Executor::visit(LiteralExpr &node) {
    if (holds_alternative<monostate>(node.value)) {
        push_value(make_shared<PyNone>());
        return;
    }
    if (holds_alternative<int>(node.value)) {
        int intVal = get<int>(node.value);
        push_value(make_shared<PyInt>(intVal));
        return;
    }
    if (holds_alternative<double>(node.value)) {
        double dblVal = get<double>(node.value);
        push_value(make_shared<PyFloat>(dblVal));
        return;
    }
    if (holds_alternative<bool>(node.value)) {
        bool boolVal = get<bool>(node.value);
        push_value(make_shared<PyBool>(boolVal));
        return;
    }
    if (holds_alternative<string>(node.value)) {
        string strVal = get<string>(node.value);
        push_value(make_shared<PyString>(strVal));
        return;
    }
    throw RuntimeError("Line X: unsupported literal type");
}
```

* `LiteralExpr` хранит некий `std::variant<std::monostate,int,double,bool,string> value`.
* Достаточно вынуть нужный вариант и положить в стек соответствующий `PyInt/ PyFloat/ PyBool/ PyString/ PyNone`.

---

### 5.18 `Executor::visit(PrimaryExpr &node)`

```cpp
void Executor::visit(PrimaryExpr &node) {
    switch (node.type) {
        case PrimaryType::LITERAL:
            node.literalExpr->accept(*this); break;
        case PrimaryType::ID:
            node.idExpr->accept(*this); break;
        case PrimaryType::CALL:
            node.callExpr->accept(*this); break;
        case PrimaryType::INDEX:
            node.indexExpr->accept(*this); break;
        case PrimaryType::PAREN:
            node.parenExpr->accept(*this); break;
        case PrimaryType::TERNARY:
            node.ternaryExpr->accept(*this); break;
        default:
            throw RuntimeError("Line X: unsupported primary expression type");
    }
}
```

* `PrimaryExpr` в AST может быть разного вида (`(a + b)`, `x`, `5`, `f(1,2)`, `arr[3]`, или тернарный оператор).
* Мы просто защитаемся: «если узел типа `LITERAL`, делегируем внутрь `LiteralExpr`; если `ID` – к `IdExpr`; если `CALL` – к `CallExpr`; и т. д.».

---

### 5.19 `Executor::visit(ListExpr &node)`

```cpp
void Executor::visit(ListExpr &node) {
    vector<ObjectPtr> elements;
    elements.reserve(node.elems.size());
    for (auto &elemExpr : node.elems) {
        if (elemExpr) {
            elemExpr->accept(*this);
            ObjectPtr value = pop_value();
            elements.push_back(value);
        } else {
            elements.push_back(make_shared<PyNone>());
        }
    }
    ObjectPtr listObj = make_shared<PyList>(move(elements));
    push_value(listObj);
}
```

* Создание литерала списка `[e1, e2, e3, …]`:

  1. Проходим каждый подузел `elemExpr` (каждый из которого выражение).
  2. Вычисляем его, складываем результат в вектор.
  3. В конце создаём `PyList(elements)` и и кладём в стек.

---

### 5.20 `Executor::visit(DictExpr &node)`

```cpp
void Executor::visit(DictExpr &node) {
    auto dictObj = make_shared<PyDict>();
    for (auto &pr : node.items) {
        Expression *keyExpr = pr.first.get();
        Expression *valueExpr = pr.second.get();

        keyExpr->accept(*this);
        ObjectPtr keyObj = pop_value();
        if (!keyObj) throw RuntimeError("… internal error: null key in dict");

        valueExpr->accept(*this);
        ObjectPtr valueObj = pop_value();
        if (!valueObj) throw RuntimeError("… internal error: null value in dict");

        try {
            dictObj->__setitem__(keyObj, valueObj);
        } catch (const RuntimeError &err) {
            throw RuntimeError("Line X TypeError: " + err.what());
        }
    }
    push_value(dictObj);
}
```

* Литерал словаря `{k1: v1, k2: v2, …}`:

  1. Создаём новый `PyDict`.
  2. Для каждой пары `(keyExpr, valueExpr)` вычисляем ключ и значение.
  3. Вызываем `dictObj->__setitem__(keyObj, valueObj)`.

     * Если ключ «нехэшируемый» (например, это `PyList`), `__setitem__` бросит `RuntimeError("…")`. Мы ловим его и «оборачиваем» в сообщение `TypeError`.
  4. Кладём готовый словарь в стек.

---

### 5.21 `Executor::visit(SetExpr &node)`

```cpp
void Executor::visit(SetExpr &node) {
    vector<ObjectPtr> tempElems;
    tempElems.reserve(node.elems.size());

    for (auto &elemExpr : node.elems) {
        elemExpr->accept(*this);
        ObjectPtr elemVal = pop_value();
        if (!elemVal) throw RuntimeError("… internal error: set element evaluated to null");

        if (dynamic_pointer_cast<PyList>(elemVal)) {
            throw RuntimeError("Line X TypeError: unhashable type: 'list'");
        }
        if (dynamic_pointer_cast<PyDict>(elemVal)) {
            throw RuntimeError("Line X TypeError: unhashable type: 'dict'");
        }
        if (dynamic_pointer_cast<PySet>(elemVal)) {
            throw RuntimeError("Line X TypeError: unhashable type: 'set'");
        }
        tempElems.push_back(elemVal);
    }

    auto setObj = make_shared<PySet>(move(tempElems));
    push_value(setObj);
}
```

* Литерал множества `{e1, e2, e3}`:

  1. Вычисляем каждый элемент — получаем `ObjectPtr`.
  2. Проверяем, что элемент «hashable» (пока в нашей упрощённой модели это значит: не `PyList`, не `PyDict`, не `PySet`). Иначе «TypeError: unhashable type: 'list'».
  3. Собираем вектор `tempElems`.
  4. `std::make_shared<PySet>(tempElems)` автоматически убирает дубли (в конструкторе `PySet` мы проходим и складываем только уникальные `repr()`).
  5. Кладём `setObj` в стек.

---

### 5.22 `Executor::visit(CondStat &node)` (if-elif-else)

```cpp
void Executor::visit(CondStat &node) {
    // 1) Вычисляем условие if
    node.condition->accept(*this);
    ObjectPtr condVal = pop_value();
    bool condIsTrue = is_truthy(condVal);

    if (condIsTrue) {
        node.ifblock->accept(*this);
        return;
    }

    // 2) Пробегаем все elif:
    for (auto &pair : node.elifblocks) {
        Expression *elifCond = pair.first.get();
        BlockStat  *elifBlock = pair.second.get();

        elifCond->accept(*this);
        ObjectPtr elifVal = pop_value();
        bool elifIsTrue = is_truthy(elifVal);

        if (elifIsTrue) {
            elifBlock->accept(*this);
            return;
        }
    }

    // 3) Если есть else — выполняем:
    if (node.elseblock) {
        node.elseblock->accept(*this);
    }
    // Иначе — ничего
}
```

* **Пошагово:**

  1. Сначала вычисляем «if <condition>». Если это истина — заходим в `ifblock` и возвращаемся (остальные `elif` / `else` не выполняются).
  2. Если `if` был ложен, последовательно проверяем все `elif` (каждый `pair.first` – это условие, а `pair.second` – блок). Как только нашли истинный `elif`, выполняем его блок и выходим.
  3. Если ни `if`, ни `elif` не сработали, а `else` есть — выполняем `elseblock`.

---

### 5.23 `Executor::visit(WhileStat &node)`

```cpp
void Executor::visit(WhileStat &node) {
    if (!node.condition) throw RuntimeError("Line X: internal error: missing condition in while");

    while (true) {
        node.condition->accept(*this);
        ObjectPtr condVal = pop_value();
        bool condIsTrue = is_truthy(condVal);

        if (!condIsTrue) break;

        if (node.body) {
            try {
                node.body->accept(*this);
            }
            catch (const BreakException &) {
                break;
            }
            catch (const ContinueException &) {
                continue;
            }
        } else {
            break;
        }
        // Переходим к следующей итерации
    }
}
```

* **Цикл `while`.**

  1. Проверяем, что есть `condition`.
  2. Входим в цикл:

     * Вычисляем условие.
     * Если ложь — выходим из цикла.
     * Если истина — выполняем тело ( `node.body->accept(*this)` ).

       * Если в теле упадёт `BreakException` ( `visit(BreakStat)` бросит его ), мы сразу выходим из цикла.
       * Если упадёт `ContinueException` (`visit(ContinueStat)`), мы переходим сразу к следующей итерации (то есть прыжок вверх, без выполнения оставшейся части тела, но здесь у нас тело одно, так что фактически просто не делаем ничего и снова вычисляем условие).
       * Если тело выполнено без исключений — снова переходим к проверке условия.

---

### 5.24 `Executor::visit(ForStat &node)`

```cpp
void Executor::visit(ForStat &node) {
    // 1) Убедимся, что каждое имя из node.iterators (список имён) есть в локальном скоупе
    for (const auto &varName : node.iterators) {
        if (!scopes.lookup_local(varName)) {
            Symbol sym;
            sym.name = varName;
            sym.type = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl = &node;
            scopes.insert(sym);
        }
    }

    // 2) Вычисляем iterable
    node.iterable->accept(*this);
    ObjectPtr iterableVal = pop_value();

    // 3) Если это PyList:
    if (auto listObj = dynamic_pointer_cast<PyList>(iterableVal)) {
        const auto &elements = listObj->getElements();
        for (size_t idx = 0; idx < elements.size(); ++idx) {
            ObjectPtr element = elements[idx];

            if (node.iterators.size() == 1) {
                Symbol *sym = scopes.lookup_local(node.iterators[0]);
                sym->value = element;
            } else {
                // Распаковка: ожидаем, что element тоже PyList  
                auto innerList = dynamic_pointer_cast<PyList>(element);
                if (!innerList) {
                    throw RuntimeError("Line X TypeError: cannot unpack non-iterable element '" + element->repr() + "'");
                }
                const auto &innerElems = innerList->getElements();
                if (innerElems.size() != node.iterators.size()) {
                    throw RuntimeError("Line X ValueError: not enough values to unpack (expected " + ... + ")");
                }
                for (size_t k = 0; k < node.iterators.size(); ++k) {
                    Symbol *sym = scopes.lookup_local(node.iterators[k]);
                    sym->value = innerElems[k];
                }
            }

            // Выполняем тело (с учётом break/continue)
            if (node.body) {
                try {
                    node.body->accept(*this);
                }
                catch (const BreakException &) {
                    break;
                }
                catch (const ContinueException &) {
                    continue;
                }
            }
        }
        return;
    }

    // 4) Если это PyString:
    if (auto strObj = dynamic_pointer_cast<PyString>(iterableVal)) {
        const string &s = strObj->get();
        for (size_t i = 0; i < s.size(); ++i) {
            auto charObj = make_shared<PyString>(string(1, s[i]));
            if (node.iterators.size() == 1) {
                Symbol *sym = scopes.lookup_local(node.iterators[0]);
                sym->value = charObj;
            } else {
                // Распаковка не поддерживается для строк (ошибка)
                throw RuntimeError("Line X TypeError: cannot unpack non-iterable element '" + charObj->repr() + "'");
            }
            if (node.body) {
                try {
                    node.body->accept(*this);
                }
                catch (const BreakException &) { break; }
                catch (const ContinueException &) { continue; }
            }
        }
        return;
    }

    // 5) Иначе бросаем TypeError
    string badType = deduceTypeName(iterableVal);
    throw RuntimeError("Line X TypeError: '" + badType + "' object is not iterable");
}
```

* **Поддерживаем «for x in iterable»:**

  * Сначала проверяем, есть ли именованные переменные `node.iterators` в локальном скоупе; если нет, создаём с `value = nullptr`.
  * Вычисляем `iterable` (например, `[1,2,3]` или `"hello"`).
  * **Случай списка (`PyList`)**:

    1. Берём `getElements()` → вектор `elements`.
    2. Для каждого `element`:

       * Если `node.iterators.size()==1` (обычная ситуация), просто `sym->value = element`.
       * Если их несколько (например, `for x, y in arr:`) – ожидаем, что `element` сам является `PyList` той же длины, и «распаковываем» по позициям.
    3. Выполняем `node.body->accept(*this)`. Ловим `BreakException` (выходим из цикла) или `ContinueException` (скипаем до следующей итерации).
  * **Случай строки (`PyString`)**:

    1. Берём `s[i]`, создаём `PyString` длины 1 для каждого символа.
    2. Делаем либо привязку к `x` (если один итератор), либо кидаем ошибку, если попытались распаковать кортеж из более чем одного имени.
    3. Аналогично выполняем тело, обрабатываем `break/continue`.
  * **Прочие типы — неитерируемые**: кидаем «TypeError: 'int' object is not iterable».

---

### 5.25 `Executor::visit(BreakStat &node)` и `Executor::visit(ContinueStat &node)`

```cpp
void Executor::visit(BreakStat &node) {
    throw BreakException{};
}

void Executor::visit(ContinueStat &node) {
    throw ContinueException{};
}
```

* `break` и `continue` в Python работают только внутри цикла.
* Мы реализовали их через исключения:

  * При встрече `break` мы кидаем `BreakException`, и ближайший внешний цикл (`visit(WhileStat)` или `visit(ForStat)`) ловит это исключение → либо делает `break` из своего `while(true)` → выходим из цикла, либо при `continue` ловим `ContinueException` → просто `continue` на следующую итерацию.
  * Если `break` встретился не внутри цикла, то ни один из `visit(WhileStat)` или `visit(ForStat)` его не поймёт, он полезет выше → и наконец окажется в `Executor::execute` → вылетим с «unhandled BreakException», что по смыслу «break outside loop».

---

### 5.26 `Executor::visit(PassStat &node)`

```cpp
void Executor::visit(PassStat &node) {
    // Ничего не делаем — просто «пустая инструкция».
}
```

* `pass` ничего не делает. В AST у него просто фиксируется «стоит pass» — и мы здесь не выполняем никаких вычислений и не портим стек.

---

### 5.27 `Executor::visit(AssertStat &node)`

```cpp
void Executor::visit(AssertStat &node) {
    if (!node.condition) throw RuntimeError("Line X: internal error: no condition in assert");
    node.condition->accept(*this);
    ObjectPtr condObj = pop_value();
    bool condTrue = is_truthy(condObj);
    if (condTrue) return; // всё хорошо — assert проходит

    // Если assert ложен:
    string message;
    if (node.message) {
        node.message->accept(*this);
        ObjectPtr msgObj = pop_value();
        message = msgObj->repr();
    }

    string errorText = "AssertionError";
    if (!message.empty()) {
        errorText += ": " + message;
    }
    throw RuntimeError("Line X " + errorText);
}
```

* `assert <condition>, <optional_message>`.
* Если условие истинно — не делаем ничего.
* Если ложно —

  1. Если есть второй аргумент `message`, вычисляем его и берём `repr()`.
  2. Бросаем `RuntimeError("Line X AssertionError: <message>")` (или просто `AssertionError`, если сообщения нет).

---

### 5.28 `Executor::visit(ExitStat &node)`

```cpp
void Executor::visit(ExitStat &node) {
    int exitCode = 0;
    if (node.expr) {
        node.expr->accept(*this);
        ObjectPtr val = pop_value();
        if (auto i = dynamic_pointer_cast<PyInt>(val)) {
            exitCode = i->get();
        }
        else if (auto b = dynamic_pointer_cast<PyBool>(val)) {
            exitCode = b->get() ? 1 : 0;
        }
        else if (auto f = dynamic_pointer_cast<PyFloat>(val)) {
            exitCode = (int)f->get();
        }
        else {
            // Любой другой объект: печатаем его repr() и выходим с кодом 1
            std::cerr << val->repr() << std::endl;
            std::exit(1);
        }
    }
    std::exit(exitCode);
}
```

* `exit(expr)` / `quit(expr)` / `sys.exit(expr)`.
* Если `expr` нет — `exit()` без аргументов → завершаем с кодом 0.
* Если `expr` есть и это `int/bool/float` — приводим к `int` и `exit(code)`.
* Если любой другой объект — печатаем `repr()` этого объекта на `stderr` и `exit(1)`.
* Этот механизм полностью завершает весь процесс через `std::exit()`.

---

### 5.29 `Executor::visit(PrintStat &node)`

```cpp
void Executor::visit(PrintStat &node) {
    if (!node.expr) {
        std::cout << std::endl;
        return;
    }
    node.expr->accept(*this);
    ObjectPtr val = pop_value();
    std::cout << val->repr() << std::endl;
}
```

* `print <expr>`:

  1. Если выражения нет (для синтаксиса `print` без аргументов), печатаем пустую строку.
  2. Иначе вычисляем `expr`, достаём его значение и `std::cout << val->repr() << std::endl`.

---

### 5.30 Пустые visit’ы для `LenStat`, `DirStat`, `EnumerateStat`

```cpp
void Executor::visit(LenStat &node) {}
void Executor::visit(DirStat &node) {}
void Executor::visit(EnumerateStat &node) {}
```

* Эти узлы были добавлены в AST, но мы не «дописали» их логику здесь, потому что они тоже реализованы через встроенные функции `len`, `dir`, `enumerate`. То есть в парсере мы могли придумать сокращённый синтаксис «`len x`» (без скобок) → сохранили в виде `LenStat`, но потом, наверное, даже не задействовали его или решили, что удобнее писать `len(x)`.
* В итоге посещение таких узлов ничего не делает – штатно они могут не встречаться, а если встретятся – просто «ничего».

---

### 5.31 `Executor::visit(ClassDecl &node)`

```cpp
void Executor::visit(ClassDecl &node) {
    shared_ptr<PyClass> parentClass = nullptr;
    if (!node.baseClasses.empty()) {
        if (node.baseClasses.size() > 1) {
            throw RuntimeError("Line X: multiple inheritance is not supported");
        }
        const string &baseName = node.baseClasses[0];
        Symbol *baseSym = scopes.lookup(baseName);
        if (!baseSym) {
            throw RuntimeError("Line X: name '" + baseName + "' is not defined");
        }
        parentClass = dynamic_pointer_cast<PyClass>(baseSym->value);
        if (!parentClass) {
            throw RuntimeError("Line X: TypeError: '" + baseName + "' is not a class");
        }
    }

    // 1) Создаём объект класса:
    auto classObj = make_shared<PyClass>(node.name, parentClass);

    // 2) Регистрируем этот класс в текущем скоупе:
    Symbol classSymbol;
    classSymbol.name  = node.name;
    classSymbol.type  = SymbolType::UserClass;
    classSymbol.value = classObj;
    classSymbol.decl  = &node;
    if (auto existing = scopes.lookup_local(node.name)) {
        *existing = classSymbol;
    } else {
        scopes.insert(classSymbol);
    }

    // 3) Обрабатываем поля (FieldDecl) внутри тела класса:
    for (auto &fieldPtr : node.fields) {
        const string &fieldName = fieldPtr->name;
        ObjectPtr initValue = make_shared<PyNone>();
        if (fieldPtr->initExpr) {
            fieldPtr->initExpr->accept(*this);
            initValue = pop_value();
        }
        auto key = make_shared<PyString>(fieldName);
        classObj->classDict->__setitem__(key, initValue);
    }

    // 4) Обрабатываем методы (FuncDecl) внутри тела класса:
    for (auto &methodPtr : node.methods) {
        FuncDecl *fdecl = methodPtr.get();

        // 4.1) Собираем defaultValues для метода:
        vector<ObjectPtr> default_values;
        for (auto &pr : fdecl->defaultParams) {
            Expression *exprNode = pr.second.get();
            if (exprNode) {
                exprNode->accept(*this);
                ObjectPtr val = pop_value();
                default_values.push_back(val);
            } else {
                default_values.push_back(make_shared<PyNone>());
            }
        }

        // 4.2) Создаём PyFunction для метода:
        auto fnObj = make_shared<PyFunction>(
            fdecl->name,
            fdecl,
            scopes.currentTable(),
            fdecl->posParams,
            move(default_values)
        );

        // 4.3) Кладём fnObj в classDict:
        auto key = make_shared<PyString>(fdecl->name);
        classObj->classDict->__setitem__(key, fnObj);
    }

    // 5) Кладём сам classObj на стек:
    push_value(classObj);
}
```

* **Алгоритм объявления класса `class C(Base): …`:**

  1. **Определяем родителя (если есть).**

     * Если указано более одного имени (множественное наследование) — сразу кидаем `RuntimeError("multiple inheritance is not supported")`.
     * Иначе берём `baseName`, кидаем `lookup(baseName)`. Если там нет символа – «name is not defined», или если символ есть, но это не `PyClass` – «TypeError: 'X' is not a class».
     * В итоге в `parentClass` у нас либо `nullptr`, либо `shared_ptr<PyClass>` на родительский класс.
  2. **Создаём новый объект `PyClass`.**

     * `auto classObj = make_shared<PyClass>(node.name, parentClass);`
     * Это уже пустой «контейнер» для атрибутов и методов.
  3. **Регистрируем имя класса в текущем (лексическом) скоупе.**

     * Это чтобы сразу после `class C: …` имя `C` стало доступно.
     * Запаковываем `classObj` в `SymbolType::UserClass`, `Symbol.value = classObj`.
  4. **Заполняем поля класса (`node.fields`).**

     * Каждый `FieldDecl` — это по сути «атрибут с начальными данными».
     * Если поле написано как `x = 5`, мы вычисляем `5` → получаем `PyInt(5)`. Если инициализатора нет (`x` просто упомянута без `=`), кладём `PyNone`.
     * Пишем в `classObj->classDict->__setitem__(PyString(fieldName), initValue)`.
  5. **Заполняем методы класса (`node.methods`).**

     * Каждый метод внутри класса – это узел AST `FuncDecl`.
     * Мы, как и при объявлении обычной функции, «замораживаем» дефолтные значения параметров прямо сейчас (п. е. аналогично верхнему `FuncDecl` мы вычисляем default-выражения) и собираем их в вектор `default_values`.
     * Создаём `PyFunction fnObj = make_shared<PyFunction>(fdecl->name, fdecl, scopes.currentTable(), fdecl->posParams, default_values)` — чтобы при вызове метода работало замыкание на текущий скоуп (где объявлен класс).
     * Кладём этот `fnObj` в `classObj->classDict`, чтобы при обращении через `instance.method(...)` работал lookup.
  6. **В конце делаем `push_value(classObj)`**, потому что объявление класса само по себе возвращает объект класса (аналогично Python, где `C = class Foo: …` приводит к тому, что `C` будет объектом класса).

* **Важно понимать,** что при объявлении класса методы *не выполняются*, их тела просто сохраняются как AST, и будут выполнены лишь при вызове (через `PyFunction::__call__`).

---

### 5.32 `Executor::visit(ListComp &node)`

```cpp
void Executor::visit(ListComp &node) {
    // 1) Вычисляем «итерируемое выражение»
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();

    // 2) Собираем сырой вектор элементов (для PyList или PyString):
    vector<ObjectPtr> rawElems;
    if (auto listObj = dynamic_pointer_cast<PyList>(iterableVal)) {
        rawElems = listObj->getElements();
    }
    else if (auto strObj = dynamic_pointer_cast<PyString>(iterableVal)) {
        for (char c : strObj->get()) {
            rawElems.push_back(make_shared<PyString>(string(1, c)));
        }
    }
    else {
        string tname = deduceTypeName(iterableVal);
        throw RuntimeError("Line X TypeError: '" + tname + "' object is not iterable");
    }

    // 3) Для каждого element в rawElems:
    vector<ObjectPtr> resultElems;
    resultElems.reserve(rawElems.size());
    for (auto &element : rawElems) {
        // 3.a) В временный scope кладём iterVar → element
        if (!scopes.lookup_local(node.iterVar)) {
            Symbol sym;
            sym.name = node.iterVar;
            sym.type = SymbolType::Variable;
            sym.value = nullptr;
            sym.decl = nullptr;
            scopes.insert(sym);
        }
        Symbol *iterSym = scopes.lookup_local(node.iterVar);
        iterSym->value = element;

        // 3.b) Вычисляем valueExpr (тело comprehension)
        node.valueExpr->accept(*this);
        ObjectPtr val = pop_value();
        resultElems.push_back(val);
    }

    // 4) Собираем PyList из resultElems и пушим:
    auto listObj = make_shared<PyList>(move(resultElems));
    push_value(listObj);
}
```

* **Этот метод реализует «list comprehension» типа `[f(x) for x in iterable]`.**

  1. Вычисляем `iterableExpr` → получаем либо список, либо строку.
  2. Собираем «сырые» элементы `rawElems`.
  3. Для каждого элемента:

     * В локальном скоупе создаём или перезаписываем переменную `iterVar` (имя, указанное в comprehension), присваивая ей `element`.
     * Вычисляем `valueExpr` (обычно какое-то выражение, зависящее от `iterVar`, например `x*2`). Результат кладём в `resultElems`.
  4. Превращаем весь `resultElems` в `PyList` и возвращаем.

---

### 5.33 `Executor::visit(DictComp &node)` и `Executor::visit(TupleComp &node)`

* По структуре почти идентичны `ListComp`, только:

  * **`DictComp`:**

    ```cpp
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();
    // собираем rawElems аналогично
    auto dictObj = make_shared<PyDict>();
    for (auto &element : rawElems) {
        // 3.a) iterVar → element
        // 3.b) вычисляем keyExpr → pop_value → keyObj
        // 3.c) вычисляем valueExpr → pop_value → valueObj
        // 3.d) dictObj->__setitem__(keyObj, valueObj)
    }
    push_value(dictObj);
    ```
  * **`TupleComp`:**

    ```cpp
    node.iterableExpr->accept(*this);
    ObjectPtr iterableVal = pop_value();
    // собираем rawElems аналогично
    vector<ObjectPtr> resultElems;
    for (auto &element : rawElems) {
        // 3.a) iterVar → element
        node.valueExpr->accept(*this);
        ObjectPtr val = pop_value();
        resultElems.push_back(val);
    }
    // в нашем упрощённом варианте «tuple comprehension» возвращаем
    // просто PyList (костыль, потому что у нас нет PyTuple)
    push_value(make_shared<PyList>(move(resultElems)));
    ```

* **Основная идея.** Генераторы списков/словари/кортежей (comprehensions) — это синтаксический сахар, который разворачивается в цикл `for`, собирающий результаты в контейнер.

---

### 5.34 `Executor::visit(LambdaExpr &node)`

```cpp
void Executor::visit(LambdaExpr &node) {
    // 1) Забираем из узла тело лямбды (Expression*), делаем move:
    unique_ptr<Expression> bodyExpr = move(node.body);

    // 2) Оборачиваем bodyExpr в ReturnStat, потому что PyFunction ожидает тело,
    //    которое бросит ReturnException (return <expr>).
    auto returnStmt = make_unique<ReturnStat>(move(bodyExpr), node.line);

    // 3) Создаем временный FuncDecl для лямбды:
    FuncDecl *lambdaDecl = new FuncDecl(
        "<lambda>",
        node.params,
        vector<pair<string,unique_ptr<Expression>>>{}, // defaultParams пустые
        move(returnStmt),
        node.line
    );

    // 4) Создаём PyFunction:
    auto lambdaObj = make_shared<PyFunction>(
        "<lambda>",
        lambdaDecl,
        scopes.currentTable(),
        node.params,
        vector<ObjectPtr>{} // пусто, defaultValues нет
    );

    // 5) Кладём lambdaObj в стек:
    push_value(lambdaObj);
}
```

* **Как сделать лямбду «как функция».**

  1. Лямбда по синтаксису `lambda x,y: expr` содержит только `Expression` в теле, а не целый `FuncDecl`. Чтобы задать её поведение как функции, которой нужно бросить `ReturnException`, мы оборачиваем `expr` в `ReturnStat(expr)`.
  2. Создаём динамически `new FuncDecl("<lambda>", node.params, {}, move(returnStmt), line)` — псевдо-AST-узел, у которого имя «`<lambda>`», позиционные параметры `node.params`, пустых defaultParams, тело из одного `ReturnStat`.
  3. Передаём этот `lambdaDecl` в конструктор `PyFunction`, чтобы получить `lambdaObj`. Обратите внимание: мы передаём текущее лексическое окружение `scopes.currentTable()`, чтобы лямбда захватила все внешние переменные (окружение).
  4. Результатом лямбда-выражения является объект `PyFunction`, который мы кладём в стек и вернём наверх.
  5. Когда кто-то потом напишет `f = lambda x: x+1`, он получит `PyFunction`, и при вызове `f(10)` сработает код в `PyFunction::__call__`, которыйрут тело (здесь это `ReturnStat(x+1)` → бросит `ReturnException(x+1)`).

---

## 6. Итоговая схема взаимосвязей и «будущее»

1. **object.hpp** – даёт нам **доменную модель рантайма**: все объекты хранятся через `std::shared_ptr<Object>`; каждый класс-«значение» (PyInt, PyString, PyList, PyFunction, PyClass, PyInstance и т. д.) расширяет `Object` и переопределяет соответствующие «магические» методы (`__add__`, `__getitem__`, `__call__`, `__getattr__`, и т. д.).

   * Благодаря OOP-полиморфизму, когда мы пишем `leftVal->__add__(rightVal)`, автоматом подставится нужная реализация ( `PyInt::__add__`, `PyString::__add__`, `PyList::__add__` и т. д.).
   * Класс `PyFunction` отвечает за «пользовательские функции», где хранится ссылка на AST и окружение.
   * Класс `PyClass` — это «класс как объект», позволяющий объявлять пользовательские классы и их экземпляры. `PyInstance` агрегирует `PyClass` и умеет хранить свои атрибуты.

2. **scope.hpp** – даёт нам **механизм хранения переменных и поиска**. Мы просто «куча» `SymbolTable`-ей, вложенных друг в друга.

   * Когда мы входим в функцию / метод / comprehension / класс / блок и решаем объявить новую переменную – мы делаем `enter_scope()`, заносим данные, а при выходе делаем `leave_scope()`.
   * `lookup` и `lookup_local` позволяют искать переменную сначала в локальном, потом постепенно подниматься к глобальному.

3. **type\_registry.hpp** – это **таблица методов на уровне типов**.

   * Мы заранее регистрируем, какие методы доступны для `PyInt`, `PyString`, `PyList`, `PyDict`, `PySet`, `PyBool`, `PyFloat` (например, `__add__`, `__getitem__`, `__contains__`).
   * В будущем можно расширять систему: когда создаётся класс, мы могли бы добавить методы из его `classDict` в `TypeRegistry`, позволяя вызывать их не только прямо через `AttributeExpr + CallExpr`, но и через более обобщённый механизм «взять метод по имени и вызвать».
   * Это удобно, если мы захотим (специально или нет) дать возможность «достать метод» через `getattr(obj, "foo")()`.

4. **pyfunction.cpp** – здесь детально сделан **вызов пользовательской функции**: Инициализация `Executor`, создание локального скоупа, привязывание аргументов, исполнение тела, обработка `return`, выход из скоупа. Это полностью в духе Python:

   * Замыкания: хранится ссылка на `scope` (лексическое окружение) в момент объявления функции.
   * Дефолтные аргументы «фиксируются» при объявлении: мы сразу же всё вычисляем, а при вызове используем уже готовые объекты.

5. **executer.cpp** – это **конкретная реализация обходчика AST** (Visitor), где каждый узел AST получает свой метод `visit(...)`:

   1. **Выражения**:

      * `PrimaryExpr`, `LiteralExpr`, `IdExpr`, `BinaryExpr`, `UnaryExpr`, `CallExpr`, `IndexExpr`, `AttributeExpr`, `TernaryExpr`, `ListExpr`, `DictExpr`, `SetExpr`, `ListComp`, `DictComp`, `TupleComp`, `LambdaExpr`
      * В каждом случае мы вызываем подузлы (`accept(*this)`), забираем результаты из стека (`pop_value()`), как нужно проверяем типы, выкидываем ошибки, создаём новые объекты (`PyInt`, `PyList`, `PyDict`, и т. д.) и возвращаем результат.

   2. **Инструкции (статические конструкции)**:

      * `ExprStat`, `AssignStat`, `CondStat`, `WhileStat`, `ForStat`, `BreakStat`, `ContinueStat`, `PassStat`, `AssertStat`, `ExitStat`, `PrintStat`, `FuncDecl`, `ClassDecl`
      * Здесь, кроме вычисления поддеревьев, ещё идет работа со `Scope`, работа с `_dict` и `_classDict_`, вставка символов в таблицу, обработка ошибок (например, дубликатов, отсутствие имени, обращение к несуществующему атрибуту), генерация исключений (`BreakException`, `ContinueException`, `ReturnException`).

   3. **Логика ошибок**:

      * При ошибках времени выполнения (везде там, где что-то не так: неправильный тип аргумента, пустой индес, отсутствие атрибута, попытка итерироваться по неитерируемому) мы бросаем `RuntimeError` с понятным текстом.
      * В `Executor::execute` мы любим ловить любые `RuntimeError` и выводить их диспетчером (`std::cerr << "RuntimeError: " << err.what()`).
      * Есть также `reporter`, который собирает семантические ошибки (например, дубли параметров) и выводит их после выполнения модуля (если они есть).

---

## Кратко об общей связке и о том, как всё «калится» в единый интерпретатор

1. **Парсер** (не показан, но собрался в `parser.cpp` и `ast.hpp`) строит AST, содержащий `TransUnit` с набором `units` (каждый `unit` – это либо `FuncDecl`, либо `ClassDecl`, либо `Stat`). Также AST умеет распознавать выражения (`LiteralExpr`, `BinaryExpr`, `CallExpr`, `LambdaExpr` и т. д.). Для каждой новой конструкции (lenStat, dirStat, enumerateStat) мы могли бы добавить новые узлы AST (`LenStat`, `DirStat`, `EnumerateStat`) и в `parser` научить его собирать эти конструкции.

2. **Executor** (в `executer.cpp`) берёт корень AST (`TransUnit`) и запускает `.accept(*this)`. За счёт паттерна Visitor у нас все узлы AST знают, какой метод у `Executor` вызывать.

3. **Scope** обеспечивает видимость переменных.

   * В глобальном скоупе сразу после создания `Executor` лежат `print`, `range`, `len`, `dir`, `enumerate` (все как `PyBuiltinFunction`).
   * При объявлении функции `leaveScope` мы вставляем `PyFunction` в скоуп.
   * При объявлении класса – вставляем `PyClass`.

4. **В процессе выполнения**:

   1. Если встретился `FuncDecl` – мы не «входим» внутрь тела, а сразу создаём объект `PyFunction` и записываем в скоуп.
   2. Если встретился `ClassDecl` – создаём `PyClass`, заполняем поля и методы, записываем в скоуп, «пушим» его в стек (в том случае, если класс утверждён как выражение, т. е. в Python можно делать `A = class C: …`, и результатом этого выражения будет сам объект класса).
   3. Если встретился `ExprStat` – вычисляем, если это вызов `print`, то он сам напечатает, иначе, например, если это `f(…)`, то выполним функцию.
   4. Если встретился `AssignStat` – заносим переменные в скоуп, либо `a = 5`, либо `list[i] = 5`, либо `obj.field = 5`.
   5. Если встретился `CallExpr` – определяем, что мы вызываем: встроенную функцию ( PyBuiltinFunction ), пользовательскую (PyFunction), либо любой объект, реализовавший `__call__`.

5. **Детальный разбор ключевых моментов**:

   * **`__call__` у PyFunction.**

     * Создаёт полностью новый `Executor` и новый скоуп.
     * Привязывает позиции и defaultы.
     * Выполняет тело и ловит `ReturnException`.
     * Выходит из скоупа и возвращает результат.

   * **`__call__` у PyClass.**

     * Создаёт новый `PyInstance(shared_from_this())`.
     * Ищет `__init__` (в `classDict` или у родителя), приводит его к `PyFunction`, вызывает `__call__` этого метода (где первым аргументом будет `self`).
     * Игнорирует возвращаемое от `__init__` (т. к. `__init__` всегда возвращает `None`).
     * Возвращает экземпляр.
     * Именно поэтому мы наследуем `enable_shared_from_this<PyClass>`, чтобы при вызове `shared_from_this()` корректно получить `shared_ptr<PyClass>`, указывающий на сам объект класса, который хранится в таблице символов.

   * **`__getattr__` у PyInstance.**

     * Сначала логика «смотреть в instanceDict» (если у экземпляра заданы свои поля через `self.x = …`).
     * Если нет – «прыгаем» в `classPtr->__getattr__`, т. е. ищем атрибут (или метод) в классе.
     * Это моделирует поведение Python, где у экземпляра сначала свои данные, потом данные класса (методы и атрибуты).

   * **`TypeRegistry`.**

     * Регистрация «встроенных» методов (`__add__`, `__getitem__`, `__contains__`…) позволяет более динамично расширять систему типов.
     * В нашем коде `TypeRegistry` используется в `Executor::visit(CallExpr)` как «последняя попытка» найти метод `__call__(self,args)` через таблицу, если у объекта нет ни `PyBuiltinFunction`, ни `PyFunction`. Это даёт возможность динамически добавлять, скажем, у какого-то пользовательского класса в `classDict["__call__"]` новый `PyFunction` и потом вызывать `instance(...)`.

   * **Области видимости ( Scope ).**

     * Каждый новый вызов функции, `for`, `while`, comprehension, класс – либо явно, либо неявно вызывает `enter_scope() / leave_scope()`, чтобы локальные переменные, параметры и локальные определения не перемешивались с внешними.
     * При использовании `lookup(name)` мы ищем «оттуда вниз вверх» по цепочке вложенных таблиц.

---

## Итоговое сведение «для защиты проекта»

1. **object.hpp:**

   * Определены базовый класс `Object` с виртуальными «магическими» методами (`__add__`, `__getitem__`, `__call__`, `__getattr__`, `__contains__` и т. д.) и чисто виртуальными `type()` и `repr()`.
   * Перечислены конкретные подклассы: `PyNone`, `PyInt`, `PyBool`, `PyFloat`, `PyString`, `PyList`, `PyDict`, `PySet`, `PyBuiltinFunction`, `PyFunction`, `PyInstance`, `PyClass`.
   * Каждый из них переопределяет те методы, которые нужны: арифметические для чисел, индексирование для списков/слов/слов, вызов для функций, атрибуты для классов/экземпляров.
   * `PyClass` наследует `std::enable_shared_from_this<PyClass>`, чтобы безопасно изнутри получить `shared_ptr<PyClass>` на себя – это нужно при создании экземпляра класса (чтобы у `PyInstance.classPtr` был правильный `shared_ptr`, который удерживает объект класса оправданным образом).

2. **scope.hpp:**

   * Класс `Scope` обёртывает стек таблиц символов (`SymbolTable`).
   * Методы: `enter_scope()`, `leave_scope()`, `insert`, `lookup`, `lookup_local`, `currentTable()`.
   * Используется везде, где мы хотим сохранить переменные и видеть доступ к вышестоящим таблицам.

3. **type\_registry.hpp:**

   * Синглтон `TypeRegistry`, который хранит мапу «тип → (метод → реализация)».
   * В конструкторе (или при вызове `registerBuiltins()`) мы заранее регистрируем, какие методы реализованы у встроенных типов ( `PyInt`, `PyBool`, `PyFloat`, `PyString`, `PyList`, `PyDict`, `PySet` ).
   * Позволяет в рантайме искать метод по имени и типу и вызывать его.

4. **pyfunction.cpp:**

   * Реализует `PyFunction::__call__` – логику вызова пользовательских функций: новый `Executor`, enter\_scope, bind параметров (позиционных и default), выполнение тела, ловля `ReturnException`, leave\_scope, возврат результата.
   * Также определены `type()`, `repr()`, геттер `getDecl()`.

5. **executer.cpp:**

   * Конструктор регистрирует все встроенные функции (`print`, `range`, `len`, `dir`, `enumerate`) как `PyBuiltinFunction` в глобальном скоупе.
   * Методы `visit(...)` охватывают все узлы AST:

     * **Литералы и простые выражения — `LiteralExpr`, `IdExpr`, `BinaryExpr`, `UnaryExpr`, `PrimaryExpr`, `CallExpr`, `IndexExpr`, `AttributeExpr`, `TernaryExpr`**
       → Вычисляют подвыражения, бросают ошибки, возвращают `ObjectPtr` через внутренний стек `valueStack` (который хранится внутри `Executor`).
     * **Коллекции и генераторы — `ListExpr`, `DictExpr`, `SetExpr`, `ListComp`, `DictComp`, `TupleComp`**
       → Собирают элементы, проверяют корректность типов, возвращают `PyList`, `PyDict`, `PySet`.
     * **Управляющие конструкции — `CondStat`, `WhileStat`, `ForStat`, `BreakStat`, `ContinueStat`, `PassStat`, `AssertStat`, `ExitStat`, `PrintStat`**
       → выполняют соответствующую логику: ветвление, циклы, break/continue через исключения, assert, выход из программы, печать, оператор `pass`.
     * **Объявления функций и классов — `FuncDecl`, `ClassDecl`**
       → При объявлении `FuncDecl` создаём `PyFunction` и вставляем его в скоуп, при `ClassDecl` создаём `PyClass`, заполняем `classDict` полями и методами, вставляем в скоуп, возвращаем сам `PyClass` как результат выражения.
     * **`LambdaExpr` → преобразуем в `PyFunction` через временный `FuncDecl` и возвращаем.**

6. **Связка всего вместе.**

   * **Парсер** (в другом модуле) строит AST.
   * **Запуск:**

     1. Создаем `Executor exec;` – в нём создаётся глобальная таблица с билтинами.
     2. Читаем AST в `TransUnit unit;`.
     3. Вызываем `exec.execute(unit);` – начинаются визиты.
     4. В ходе этого: объявляем все функции/классы, выполняем глобальные инструкции (присваивания, вызовы `print`, определения других функций), пока не закончатся топ-уровневые узлы или не выпадет ошибка.
     5. Если в процессе выполнения в любой момент вызовется `exit()`, произойдёт `std::exit(code)`.
   * Таким образом, получаем полностью функционирующий интерпретатор Python-образного языка, где поддерживаются:

     * Примитивные типы (`int`, `float`, `bool`, `str`, `list`, `dict`, `set`),
     * Арифметика, логика, сравнения, `in`, `not in`,
     * Индексирование и присваивание по индексу,
     * Условные операторы, циклы, break/continue, assert, exit, print, pass,
     * Функции (встроенные + пользовательские + лямбды),
     * Классы и экземпляры, наследование одного класса,
     * Генераторы списков/словарей/кортежей,
     * Динамическая типизация через OOP-полиморфизм и `TypeRegistry`.

Весь код сделан «интуитивно» – видно, как он мог бы развиваться дальше:

* Если нужно добавить, скажем, кортежи, просто написать `class PyTuple` → унаследовать от `Object` и реализовать методы.
* Если нужно добавить более сложные случаи «operator overloading» или новые встроенные функции – добавляем в `TypeRegistry` или в конструктор `Executor`.
* Если добавить обработку декораторов, импортов, исключений, надо будет дописать соответствующие узлы AST и методы `visit(...)`. Но общая архитектура уже готова.

---

### Честный ответ «преподавателю»

> **object.hpp**
> — это наша реализация всей системы типов на рантайме: от `PyInt`, `PyBool`, `PyFloat`, `PyString`, `PyList`, `PyDict`, `PySet` до `PyFunction`, `PyBuiltinFunction`, `PyClass`, `PyInstance` и `PyNone`.
> — Все они наследуют от общего базового `Object`, который задаёт виртуальные методы (`__add__`, `__getitem__`, `__call__`, `__getattr__`, `__contains__`, `__setitem__`, `__setattr__`, `type()`, `repr()`).
> — Семантика в духе Python:
> • Арифметические операции пытаются динамически приводить операнды (int+float, bool+int, int\*list → повторение списка и т. д.).
> • Индексирование для string, list, dict.
> • Атрибуты для классов и экземпляров.
> • Функции хранят AST и энвайронмент, могут вызываться через `__call__`.
> • Классы умеют делать наследование (единственное), создавать экземпляры, вызывать `__init__`.
> — `std::enable_shared_from_this<PyClass>` нужен, чтобы `PyClass` мог при создании экземпляра изнутри взять корректный `shared_ptr<PyClass>` на самого себя и передать его в `PyInstance`.

> **scope.hpp**
> — обёртка над стеком таблиц символов `SymbolTable`.
> — Методы: `enter_scope()`, `leave_scope()`, `insert()`, `lookup()`, `lookup_local()`.
> — При входе в функцию / метод / класс / comprehension мы делаем `enter_scope()`, при выходе — `leave_scope()`. Это обеспечивает лексическую область видимости.

> **type\_registry.hpp**
> — Синглтон `TypeRegistry`, где мы регистрируем для каждого `typeid(...)` ( `PyInt`, `PyBool`, `PyFloat`, `PyString`, `PyList`, `PyDict`, `PySet` ) список «методов» (`__add__`, `__getitem__`, `__contains__` и т. д.).
> — Встроенные методы упрощают вызов: например, при выполнении `val->__getattr__("__contains__")` мы можем через `TypeRegistry::getMethod(val->type(),"__contains__")` получить лямбду, которая сделает `val->__contains__(…)`.
> — Это даёт гибкий механизм «привязывания» методов к типам и их динамическое расширение.

> **pyfunction.cpp**
> — Реализация `PyFunction`, в частности `__call__`:
>
> 1. Создаётся новый `Executor` (с уже зарегистрированными билтиными).
> 2. `enter_scope()` для создания локальной области (параметры + локальные переменные).
> 3. Привязываются позиционные аргументы и default-аргументы.
> 4. Выполняется тело `decl->body->accept(exec)`. Если встретился `return` – кидается `ReturnException`, значение возвращается, иначе возвращается `None`.
> 5. `leave_scope()`.
>    — `type()` возвращает `typeid(PyFunction)`; `repr()` строит `<function name at pointer>`.

> **executer.cpp**
> — Ключевой класс `Executor` с методами `visit(...)` для совершенно всех узлов AST:
> • **Вспомогательные функции:**
> – `is_truthy(ObjectPtr&)` — проверка на «истину» в духе Python.
> – `deduceTypeName(ObjectPtr&)` — для формирования сообщений об ошибках.
> • **Конструктор:**
> – `TypeRegistry::instance().registerBuiltins();`
> – Создаёт `PyBuiltinFunction` для `print`, `range`, `len`, `dir`, `enumerate` и сразу кладёт их в глобальный скоуп (`scopes.insert(...)`).
> • **evaluate/execute:**
> – `evaluate(expr)` – запускает обход AST для выражения и возвращает его результат.
> – `execute(TransUnit&)` – запускает обход всего модуля, ловим `RuntimeError`, потом печатаем накопленные ошибки из `reporter`, если есть.
> • **visit(FuncDecl):**
> – Проверка дубликатов параметров.
> – Вычисление default-значений при объявлении.
> – Создание `PyFunction` и вставка в символ таблицу (без входа в тело!).
> • **visit(BlockStat):** просто проходим все инструкции подряд.
> • **visit(ExprStat):** вычисляем, но «скидываем» результат (иначе стек накапливался бы напрасно).
> • **visit(AssignStat):**
> – Вычисление правой части.
> – Если слева имя (`IdExpr`) – или создаём новую переменную, или обновляем старую.
> – Если индекс выражение (`IndexExpr`) – вычисляем базу и индекс, вызываем `__setitem__`.
> – Если атрибут (`AttributeExpr`) – вычисляем базу, вызываем `__setattr__`.
> – Иначе — ошибка.
> • **visit(IdExpr):** lookup в `scopes.lookup`, если нет → ошибка, если есть, но `value == nullptr` → «referenced before assignment», иначе push\_value.
> • **visit(BinaryExpr):**
> – Сначала вычисляем оба операнда (сохранив их результаты через `pop_value()`).
> – Если оператор арифметический (`+ - * /`) → вызываем соответствующий `__add__`, `__sub__` и т. д.
> – Если сравнение (`== != < …`) – сравниваем `repr()`.
> – Если `and/or` – применяем правила Python.
> – Если `in / not in` – вызываем `right->__contains__(left)` (при ошибке кидаем TypeError).
> • **visit(UnaryExpr):** реализуем `+`, `-`, `not` (с проверкой типов).
> • **visit(CallExpr):**
> 1\. Вычисляем «callee».
> 2\. Вычисляем аргументы.
> 3\. Если `PyBuiltinFunction` – вызываем `__call__`.
> 4\. Иначе если `PyFunction` – либо пользовательскую, либо метод класса: проверяем число аргументов, `enter_scope()`, bind params, выполнить тело, catch `ReturnException`, `leave_scope()`, `push_value(returnValue)`.
> 5\. Иначе – пробуем `callee->__call__(args)` напрямую, если оно есть, иначе «TypeError: object is not callable».
> • **visit(ReturnStat):**
> – Если есть `expr` – вычисляем её, `pop_value()`, `throw ReturnException` с этим значением; иначе – `throw ReturnException(PyNone)`.
> • **visit(IndexExpr):** см. выше.
> • **visit(AttributeExpr):**
> – Вычисляем `base`, вызываем `base->__getattr__(name)`.
> – При ошибке формируем «TypeError: 'TYPE' object has no attribute 'name'».
> • **visit(TernaryExpr):** реализует `<a> if <cond> else <b>`.
> • **visit(LiteralExpr), visit(PrimaryExpr):** см. раздел 5.17, 5.18.
> • **visit(ListExpr), visit(DictExpr), visit(SetExpr):** создают соответствующие объекты `PyList`, `PyDict`, `PySet`, проверяют ошибки (ключи, hashable, диапазоны), push на стек.
> • **visit(CondStat), visit(WhileStat), visit(ForStat), visit(BreakStat), visit(ContinueStat), visit(PassStat), visit(AssertStat), visit(ExitStat), visit(PrintStat):**
> – `CondStat`: стреляет `if/elif/else`.
> – `WhileStat`: `while`, с поддержкой break/continue через исключения.
> – `ForStat`: `for var in iterable`, поддерживаем `list` и `str`, деструктуризацию (распаковку) для списочных элементов.
> – `BreakStat`, `ContinueStat`: бросают `BreakException`/`ContinueException`.
> – `PassStat`: ничего не делает.
> – `AssertStat`: проверяет условие, кидает `AssertionError`.
> – `ExitStat`: завершает программу.
> – `PrintStat`: печатает значение.
> • **visit(LenStat), visit(DirStat), visit(EnumerateStat):** пустые, потому что вместо них можно было бы писать `len(x)`, `dir(x)`, `enumerate(x)`.
> • **visit(ClassDecl):** создаёт `PyClass` (узнаёт родителя, проверяет единственное наследование), регистрирует имя класса, наполняет поля (FieldDecl) и методы (FuncDecl), push `classObj`.
> • **visit(ListComp), visit(DictComp), visit(TupleComp):** реализуют comprehension.
> • **visit(LambdaExpr):** превращает лямбда-выражение в нереальный `FuncDecl`, на базе которого создаётся `PyFunction`, который и кладётся в стек.

В результате у нас есть **полноценный интерпретатор**, который можно расширять. Самое важное – архитектура чистая:

* AST + Visitor – чётко разделены синтаксис/семантика.
* `Object` и его подклассы – объектная модель данных.
* `Scope` и `SymbolTable` – система областей видимости.
* `TypeRegistry` – динамическая привязка методов по типу.
* `Executor` – реализация семантики каждого узла AST, пошагово.

Надеюсь, это подробное описание каждого файла и каждого ключевого метода поможет тебе уверенно рассказать преподавателю, как и почему всё связано.
