#include "cps.h"
#include "parse.h"
#include "stdlib.h"

#define LOUISP_CODE(...) #__VA_ARGS__

/* This is pretty much a hack.
 * Comments are... sort-of... supported. You need to terminate them with the
 * character sequence \n (that's a literal backslash and the n character) because
 * of the way stringification works. */
static const char louisp_stdlib[] = LOUISP_CODE(
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
          (list (quote if) (car (car condns)) ; test\n
               (car (cdr (car condns))) ; result\n
               (cons (quote cond) (cdr condns)))))))

(define map (lambda (fn lst)
  (if (null? lst)
      nil
      (cons (fn (car lst)) (map fn (cdr lst))))))

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
        (#t (list (quote if) (car args) (cons (quote and) (cdr args)))))))

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

; extra layer of abstraction because the long form is only defined \n
; on the cps-style function \n
(define call/cc (lambda (fun) (call-with-current-continuation fun)))

; to help with copy/pasting from repl.it \n
(define define-macro (macro (nameargs . body)
  (list (quote define) (car nameargs) (cons (quote macro) (cons (cdr nameargs) body)))))
);

void add_stdlib(struct env *env) {
	struct input i = input_from_string(louisp_stdlib);
	while ((gc_current_obj = parse(&i)) != NULL) {
		run_cps(&gc_current_obj, env);
	}
}