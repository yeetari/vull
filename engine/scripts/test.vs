;; Test comment.

(define foo 5)
(define list1 '(+ foo bar))
(define bar (+ foo foo))
(collect-garbage)

(define add +)

(seq
 (+ 1 2)
 (add 1 foo bar -1))
