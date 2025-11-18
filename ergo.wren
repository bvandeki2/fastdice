class RangeDistNode {
  // domain fields
  min { _min }
  max { _max }

  // AST fields
  op { _op }
  args { _args }

  +(rhs) {
    // TODO: adds commute, so we'd like to combine up to n for a single + op
    // add is a very common operation on dice, so worth optimizing
    return RangeDistNode.new(
      min + rhs.min,
      max + rhs.max,
      "+",
      [this, rhs]
    )
  }
  
  - {
    return RangeDistNode.new(-max, -min, [])
  }
  
  -(rhs) {
    return this + (-rhs)
  }
  
  toString { op + "(" + min.toString + ".." + max.toString }

  construct new(min, max, op, args) {
    if (min > max) {
        Fiber.abort("min > max") 
    }
    _min = min
    _max = max
    _op = op
    _args = args
  }

  static uniform(min, max) {
    return RangeDistNode.new(min, max, "uniform", [])
  }
}

var d =  Fn.new { |sides| RangeDistNode.uniform(1, sides) }
var die = d.call(6)
System.print(die.toString)