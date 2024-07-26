(def! my-fun ((fn [x] (fn [y] (seq (collect-garbage) (+ x y)))) 5))
(my-fun 6)
