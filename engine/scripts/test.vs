;; Test comment.

(def! foo 5)
(def! list1 '(+ foo bar))
(def! bar (+ foo foo))
(collect-garbage)

(def! add +)

(seq
 (+ 1 2)
 (add 1 foo bar -1))
