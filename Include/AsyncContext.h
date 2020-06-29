#ifndef ASYNCCONTEXT_H
#define ASYNCCONTEXT_H

template<
class A = int, 
class B = int,
class C = int,
class D = int,
class E = int,
class F = int,
class G = int,
class H = int>
class AsyncContext {
public:
	A item1;
	B item2;
	C item3;
	D item4;
	E item5;
	F item6;
	G item7;
	H item8;

	AsyncContext(A a = 0,
		B b = 0,
		C c = 0,
		D d = 0,
		E e = 0,
		F f = 0,
		G g = 0,
		H h = 0)
		:item1(a), 
		item2(b), 
		item3(c), 
		item4(d), 
		item5(e),
		item6(f),
		item7(g),
		item8(h){

	}
};

#endif