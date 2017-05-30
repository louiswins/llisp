#include "cps.h"
#include "parse.h"
#include "stdlib.h"

#define LLISP_CODE(...) #__VA_ARGS__

/* This is pretty much a hack.
 * Comments are... sort-of... supported. You need to terminate them with the
 * character sequence \n (that's a literal backslash and the n character) because
 * of the way stringification works. */
static const char llisp_stdlib[] = LLISP_CODE(
(define nil ())
(define null? (lambda (x) (eq? x nil)))

(define zero? (lambda(n) (= 0 n)))

(define #t (= 0 0))
(define #f (= 0 1))

; if in terms of cond \n
;(define if (macro (cnd then . els)
;  (cons (quote cond) (cons (list cnd then)
;                           (cond ((null? els) ())
;                                 (#t (list (cons #t els)))))))) \n

; cond in terms of if \n
(define cond (macro condns
  (if (null? condns) #f
      (if (null? (cdr (car condns)))
          (let ((sym (gensym))) ; (hygiene :)\n
            (list (quote let) (list (list sym (car (car condns))))
               (list (quote if) sym sym (cons (quote cond) (cdr condns)))))
          (if (eq? (car (car condns)) (quote else))
              (cons (quote begin) (cdr (car condns))) ; just do it if you saw else \n
              (list (quote if) (car (car condns)) ; test \n
                    (cons (quote begin) (cdr (car condns))) ; result \n
                    (cons (quote cond) (cdr condns))))))))

(define map (lambda (fn lst)
  (if (null? lst)
      nil
      (cons (fn (car lst)) (map fn (cdr lst))))))
(define for-each (lambda (fn lst)
  (map fn lst)
  nil))

(define filter (lambda (fn lst)
  (define filter-helper (lambda (fn lst accum)
    (cond ((null? lst) accum)
          ((fn (car lst)) (filter-helper fn (cdr lst) (cons (car lst) accum)))
          (else (filter-helper fn (cdr lst) accum)))))
  (reverse (filter-helper fn lst nil))))

(define foldl (lambda (fn init lst)
  (if (null? lst)
      init
      (foldl fn (fn init (car lst)) (cdr lst)))))

(define foldr (lambda (fn init lst)
  (if (null? lst)
      init
      (fn (car lst) (foldr fn init (cdr lst))))))

(define list (lambda args args))

(define list? (lambda (lst)
  (cond ((null? lst))
        ((pair? lst) (list? (cdr lst))))))

(define let (macro (bindings . body)
  (cons ; application \n
    (list ; lambda defn \n
      (quote lambda)
      (map car bindings)
      . body)
    (map (lambda (x) (car (cdr x))) bindings)))) ; args \n

(define let* (macro (bindings . body)
  (cond ((null? bindings) (cons (quote begin) body))
        ((null? (cdr bindings)) (cons (quote let) (cons bindings body)))
        (#t (list (quote let) (list (car bindings))
          (cons (quote let*) (cons (cdr bindings) body)))))))

(define letrec (macro (bindings . body)
  (list (cons (quote lambda) (cons ()
    (foldr (lambda (binding rest) (cons (cons (quote define) binding) rest)) body bindings))))))

(define reverse (lambda (lst)
  (foldl (lambda (rest cur) (cons cur rest)) () lst)))

; matches python (without step) \n
(define range (lambda (a . b)
  (define range-helper (lambda (start cur rest)
    (if (< cur start)
        rest
        (range-helper start (- cur 1) (cons cur rest)))))
  (if (null? b)
      (range-helper 0 (- a 1) nil)
      (range-helper a (- (car b) 1) nil))))

(define and (macro args
  (cond ((null? args) #t)
        ((null? (cdr args)) (car args)) ; final symbol \n
        (else (list (quote if) (car args) (cons (quote and) (cdr args)))))))

(define or (macro args
  (if (null? args)
      #f
      (cons (quote cond) (map list args)))))

(define not (lambda (x) (if x #f #t)))

(define append (lambda (xs ys)
  (foldr cons ys xs)))

(define nth (lambda (n lst)
  (cond ((null? lst) (begin (display (quote list-too-short))(newline)(error)))
        ((zero? n) (car lst))
        (#t (nth (- n 1) (cdr lst))))))

(define pi 3.14159265358979323846)

; Disgustingly inefficient \n
; Also not guaranteed to work since we use doubles everywhere. Who knows about fmod. \n
(define prime? (lambda (n)
  (define prime-helper (lambda (k)
    (cond ((<= k 1) #t)
          ((= (% n k) 0) #f)
          (else (prime-helper (- k 2))))))
  (cond ((< n 2) #f)
        ((= n 2) #t)
        ((= 0 (% n 2)) #f)
        (else (prime-helper (- n 2))))))

(define length (lambda (lst)
  (define length-helper (lambda (n lst)
    (cond ((null? lst) n)
          ((pair? lst) (length-helper (+ n 1) (cdr lst)))
          (#t (begin (display (quote not-a-list))(newline)(error))))))
  (length-helper 0 lst)))

(define equal? (lambda (a b)
  (cond ((eq? a b))
        ((and (pair? a) (pair? b)) (and (equal? (car a) (car b))
                                        (equal? (cdr a) (cdr b)))))))

(define call/cc call-with-current-continuation)

; to help with copy/pasting from repl.it \n
(define define-macro (macro (nameargs . body)
  (list (quote define) (car nameargs) (cons (quote macro) (cons (cdr nameargs) body)))))
);

void add_stdlib(struct env *env) {
	struct input i = input_from_string(llisp_stdlib);
	struct obj *obj;
	while ((obj = parse(&i)) != NULL) {
		run_cps(obj, env);
	}
}