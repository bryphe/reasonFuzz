/*
  Differences:
      - Allow_match doesn't use action.
 */

open Types;

/*****************************************************************************/

let fuzzyScore =
    (
      lChar: Char.t,
      lId: int,
      lPrevChar: option(Char.t),
      pChar: Char.t,
      pId: int,
      _pPrevChar: option(Char.t),
    ) => {
  let score = ref(0);

  let lPrevCharType = CharType.charTypeOf(lPrevChar);

  if (pChar == lChar && CharType.charTypeOf(Some(pChar)) == CharType.Upper) {
    score := score^ + PathScore.default.bonusUpperMatch;
  } else {
    score := score^ + PathScore.default.penaltyCaseUnmatched;
  };

  if (lId == 0 && CharType.charTypeOf(Some(lChar)) != CharType.Upper) {
    score^;
  } else {
    if (lId == 0 && CharType.charTypeOf(Some(lChar)) == CharType.Upper) {
      score := score^ + PathScore.default.bonusCamel;
    };

    if (lPrevCharType == CharType.Separ) {
      score := score^ + PathScore.default.bonusSeparator;
    };

    if (CharType.charTypeOf(lPrevChar) == CharType.Lower
        && CharType.charTypeOf(Some(lChar)) == CharType.Upper) {
      score := score^ + PathScore.default.bonusCamel;
    };

    if (pId == 0) {
      score :=
        score^
        + max(
            lId * PathScore.default.penaltyLeading,
            PathScore.default.penaltyMaxLeading,
          );
    };

    score^;
  };
};

/*****************************************************************************/

let dealWithScoreRow =
    (
      index: int,
      current: MatchingStatus.t,
      next: MatchingStatus.t,
      prev: MatchingStatus.t,
      scoreBeforeIndex: int,
    ) =>
  if (current.index < next.index) {
    let adjNum = next.index - current.index - 1;
    let finalScore = ref(current.score + next.score);

    if (adjNum == 0) {
      finalScore := finalScore^ + PathScore.default.bonusAdjacency;
    } else {
      finalScore := finalScore^ + PathScore.default.penaltyUnmatched * adjNum;
    };

    (index, finalScore^, adjNum);
  } else {
    (prev.backRef, scoreBeforeIndex, prev.adjNum);
  };

let compareTwoScoreTuples =
    (scoreTuple1: (int, int, int), scoreTuple2: (int, int, int)) => {
  let (_, finalScore1, _) = scoreTuple1;
  let (_, finalScore2, _) = scoreTuple2;

  compare(finalScore1, finalScore2);
};

let buildGraph = (line: string, pattern: string) => {
  let lineLen = String.length(line);
  let patternLen = String.length(pattern);

  let scores: ref(array(array(MatchingStatus.t))) = ref([||]);

  let previousMatchedId = ref(-1);
  let pPrevChar: ref(option(Char.t)) = ref(None);
  let validMatch = ref(true);

  /* Initialise the match positions and inline scores. */
  let pId = ref(0);
  while (pId^ < patternLen - 1 && validMatch^) {
    let pChar = Char.lowercase_ascii(pattern.[pId^]);

    let statuses = ref([||]);
    let lPrevChar: ref(option(Char.t)) = ref(None);

    let lId = ref(0);
    while (lId^ < lineLen - 1 && validMatch^) {
      let lChar = Char.lowercase_ascii(line.[pId^]);

      if (lChar == pChar && lId^ > previousMatchedId^) {
        /* TODO:
           statuses.append(MatchStatus Object with score)
           */
        let score =
          fuzzyScore(lChar, lId^, lPrevChar^, pChar, pId^, pPrevChar^);
        let newMatch =
          MatchingStatus.create(
            ~index=lId^,
            ~score,
            ~finalScore=score,
            ~adjNum=1,
            ~backRef=0,
          );
        statuses := Array.append(statuses^, [|newMatch|]);
      };

      lPrevChar := Some(lChar);
      lId := lId^ + 1;
    };

    if (Array.length(statuses^) == 0) {
      validMatch := false;
    } else {
      previousMatchedId := statuses^[0].index;
      scores := Array.append(scores^, [|statuses^|]);
      pPrevChar := Some(pChar);
    };

    pId := pId^ + 1;
  };

  pId := 1;
  while (pId^ < Array.length(scores^) && validMatch^) {
    let (firstHalf, lastHalf) = Helpers.splitArray(scores^, pId^);

    let previousRow = firstHalf[Array.length(firstHalf) - 1];
    let currentRow = lastHalf[0];

    for (index in 0 to Array.length(currentRow)) {
      let next = currentRow[index];
      let prev = index > 0 ? currentRow[index - 1] : MatchingStatus.default;

      let scoreBeforeIndex = ref(prev.finalScore - prev.score + next.score);
      scoreBeforeIndex :=
        scoreBeforeIndex^
        + PathScore.default.penaltyUnmatched
        * (next.index - prev.index);

      if (prev.adjNum == 0) {
        scoreBeforeIndex :=
          scoreBeforeIndex^ - PathScore.default.bonusAdjacency;
      };

      let result =
        Array.mapi(
          (index, item) => dealWithScoreRow(index, item, next, prev, scoreBeforeIndex^),
          previousRow,
        );
      Array.fast_sort(compareTwoScoreTuples, result);
      /* TODO: Check this sorts the way I expect! Ie, 0 is the max score. */
      let (backRef, score, adjNum) = result[0];

      let currentStatus =
        if (index > 0 && score < scoreBeforeIndex^) {
          MatchingStatus.create(
            ~index=next.index,
            ~score=next.score,
            ~finalScore=scoreBeforeIndex^,
            ~adjNum,
            ~backRef,
          );
        } else {
          MatchingStatus.create(
            ~index=next.index,
            ~score=next.score,
            ~finalScore=score,
            ~adjNum,
            ~backRef,
          );
        };

      currentRow[index] = currentStatus;
    };

    pId := pId^ + 1;
  };

  if (validMatch^ == false) {
    None;
  } else {
    Some(scores^);
  };
};

/*****************************************************************************/

let compareMatchingStatus =
    (matchStatus1: MatchingStatus.t, matchStatus2: MatchingStatus.t) => {
  compare(matchStatus1.finalScore, matchStatus2.finalScore);
};

let getBestScore = (scoresArray: array(MatchingStatus.t)) => {
  Array.fast_sort(compareMatchingStatus, scoresArray)
  scoresArray[0].finalScore
};

let fuzzyMatch = (~line: string, ~pattern: string) => {
  if (String.length(pattern) == 0) {
    None
  } else {
    let scores = buildGraph(line, pattern);

    let finalScore = switch(scores) {
    | Some(scoreArray) => getBestScore(scoreArray[Array.length(scoreArray) - 1])
    | None => 0;
    };

    Some(MatchResult.create(finalScore))
  };
};